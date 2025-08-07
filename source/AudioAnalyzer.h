#pragma once

#include <JuceHeader.h>
#include <mutex>
#include <vector>

/*  AudioAnalyzer.h

This file defines the AudioAnalyzer class, which is responsible for 
analyzing audio data using FFT and CQT transforms. It can compute 
frequency bands, magnitudes, and pan indices based on the audio input. 
The class also manages a worker thread for processing audio blocks 
asynchronously.
*/

struct frequency_band {
    float frequency; // Band frequency in Hertz
    float amplitude; // Range 0 - 1 ...?
    float pan_index; // -1 = far left, +1 = far right
};

typedef std::vector<juce::dsp::Complex<float>> fft_buffer_t;
typedef std::array<std::vector<float>, 2> magnitude_spectrums_t;

enum Transform { FFT, CQT };
enum PanMethod { level_pan, time_pan, both };

class AudioAnalyzer
{
public:
    //=========================================================================
    AudioAnalyzer();
    ~AudioAnalyzer();

    // Must be called before analyzeBlock()
    void prepare();

    // Called by audio thread
    void enqueueBlock(const juce::AudioBuffer<float>* buffer);

    // Called by GUI thread to get latest results
    std::vector<frequency_band> getLatestResults() const;

    void setSampleRate(double newSampleRate);
    void setSamplesPerBlock(int newSamplesPerBlock);
    void setTransform(Transform newTransform);
    void setPanMethod(PanMethod newPanMethod);
    void setFFTOrder(float newFFTOrder);
    void setMinFrequency(float newMinFrequency);
    void setNumCQTBins(float newNumCQTBins);

    bool getPrepared() const { return isPrepared.load(); }
    void setPrepared(bool prepared) { isPrepared.store(prepared); }


    //=========================================================================
    class AnalyzerWorker;

    void stopWorker();
    
private:
    //=========================================================================
    /* Analysis functions */

    void prepareBandpassFilters();

    void computeFFT(const juce::AudioBuffer<float>& buffer,
                    std::array<fft_buffer_t, 2>& outSpectra);
    void computeILDs(std::array<std::vector<float>, 2>& magnitudes,
                     int numBands, std::vector<float>& outPan);
    void computeCQT(const juce::AudioBuffer<float>& buffer,
                    std::array<fft_buffer_t, 2> ffts,
                    std::array<std::vector<float>, 2>& cqtMags);
    float gccPhatDelayPerBand(const float* x, const float* y, 
                              juce::dsp::IIR::Filter<float>& bandpassLeft, 
                              juce::dsp::IIR::Filter<float>& bandpassRight);
    void computeGCCPHAT_ITD(const juce::AudioBuffer<float>& buffer, 
                            int numBands, std::vector<float>& panIndices);

    void analyzeBlock(const juce::AudioBuffer<float>& buffer);

    //=========================================================================
    /* Parameters */

    int samplesPerBlock;
    double sampleRate;

    enum Transform transform;
    enum PanMethod panMethod;
    
    int fftOrder; // FFT order = log2(fftSize) - not used at the moment
    float minCQTfreq;
    int numCQTbins;

    //=========================================================================

    std::vector<float> window; // Hann window of length fftSize

    int fftSize;
    std::unique_ptr<juce::dsp::FFT> fft; // JUCE FFT engine
    fft_buffer_t fftBuffer;
    int numFFTBins; // Number of useful bins from FFT
    // float maxExpectedMag;
    
    // CQT stuff
    std::vector<float> centerFrequencies;

    // float Qtarget = 1.0f / (std::pow(2.0f, 1.0f / numCQTbins) - 1.0f);
    // float Q = jlimit(1.0f, 12.0f, Qtarget);  // Clamp Q
    float Q = 1.0f;

    // Each filter is a complex-valued kernel vector (frequency domain)
    std::vector<std::vector<std::complex<float>>> cqtKernels;

    // Bandpass filters for each CQT center frequency
    std::vector<juce::dsp::IIR::Filter<float>> leftBandpassFilters;
    std::vector<juce::dsp::IIR::Filter<float>> rightBandpassFilters;
    
    // float maxITD = 0.09f / 343.0f; // ~0.00026 sec (0.09m from center of head 
                                   // to each ear, 343 m/s speed of sound)

    float maxITD = 0.00066f; // max itd according to some sources

    std::vector<float> weights;

    std::vector<frequency_band> results; // Must be sorted by frequency!!!
    mutable std::mutex resultsMutex;

    // Worker 
    std::unique_ptr<AnalyzerWorker> worker;

    // Atomic flag to indicate if the analyzer is prepared
    std::atomic<bool> isPrepared { false };
};


//=============================================================================
/*  A class to manage to the worker thread that performs the actual 
    audio analysis. 
*/
class AudioAnalyzer::AnalyzerWorker
{
public:
    AnalyzerWorker(int numSlots, int blockSizeIn, AudioAnalyzer& parent)
        : fifo(numSlots), 
          buffers(numSlots), 
          parentAnalyzer(parent)
    {
        // Pre-allocate buffers
        for (auto& buf : buffers)
            buf.setSize(2, blockSizeIn);

        // Launch background thread
        shouldExit = false;
        // thread = std::thread([this] { run(); });
    }

    ~AnalyzerWorker()
    {
        // Signal thread to exit
        shouldExit = true;
        cv.notify_one();
        if (thread.joinable())
            thread.join();
    }

    void start()
    {
        shouldExit = false;
        thread = std::thread([this] { run(); });
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            shouldExit = true;
        }
        cv.notify_one(); // Wake up the thread
        if (thread.joinable())
        {
            thread.join(); // Wait for it to finish
        } 
    }

    /* Called on audio thread: enqueue a copy of 'input' into the FIFO. */
    void pushBlock(const juce::AudioBuffer<float>& input)
    {
        int start1, size1, start2, size2;

        // If no free space, drop the block
        if (fifo.getFreeSpace() == 0)
            return;

        fifo.prepareToWrite(1, start1, size1, start2, size2);

        // size1 should be 1, and start2/size2 unused
        // Copy into pre-allocated buffer slot
        // We assume input has same numChannels and blockSize
        buffers[start1].makeCopyOf(input);
        fifo.finishedWrite(size1);

        // Notify worker thread that new data is available
        std::lock_guard<std::mutex> lock(mutex);
        cv.notify_one();
    }

private:
    // Background thread main loop 
    void run()
    {
        while (!shouldExit)
        {
            int start1, size1, start2, size2;
            // Check if there is data ready
            if (fifo.getNumReady() > 0)
            {
                fifo.prepareToRead(1, start1, size1, start2, size2);
                if (size1 < 0) continue;

                if (parentAnalyzer.getPrepared())
                    parentAnalyzer.analyzeBlock(buffers[start1]);
                
                fifo.finishedRead(size1);
            }
            else
            {
                // If no data ready, wait briefly or until notified
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait_for(lock, std::chrono::milliseconds(2));
            }
        }
    }

    juce::AbstractFifo fifo;
    std::vector<juce::AudioBuffer<float>> buffers;
    std::thread thread;
    std::atomic<bool> shouldExit {false};
    std::condition_variable cv;
    std::mutex mutex;

    AudioAnalyzer& parentAnalyzer;
};