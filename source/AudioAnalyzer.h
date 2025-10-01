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

using Complex = juce::dsp::Complex<float>;

enum Transform { FFT, CQT };
enum PanMethod { level_pan, time_pan, both };
enum FrequencyWeighting { none, A_weighting };

class AudioAnalyzer
{
public:
    //=========================================================================
    AudioAnalyzer();
    ~AudioAnalyzer();

    // Must be called before analyzeBlock()
    void prepareToPlay(int samplesPerBlock, double sampleRate);
    void prepareToPlay(); // Uses current sampleRate and samplesPerBlock

    // Called by audio thread
    void enqueueBlock(const juce::AudioBuffer<float>* buffer);

    // Called by GUI thread to get latest results
    std::vector<frequency_band> getLatestResults() const;

    void setTransform(Transform newTransform);
    void setPanMethod(PanMethod newPanMethod);
    void setNumCQTBins(int newNumCQTBins);
    void setMinFrequency(float newMinFrequency);
    void setMaxAmplitude(float newMaxAmplitude);
    void setThreshold(float newThreshold);
    void setFreqWeighting(FrequencyWeighting newFreqWeighting);

    bool getPrepared() const { return isPrepared.load(); }
    void setPrepared(bool prepared) { isPrepared.store(prepared); }

    //=========================================================================
    class AnalyzerWorker;

    void stopWorker();
    
private:
    //=========================================================================
    /* Setup functions */

    void setupFFT();
    void setupCQT();
    void setupAWeights(const std::vector<float>& freqs,
                       std::vector<float>& weights);
    void setupPanWeights();
    
    /* Analysis functions */

    void analyzeBlock(const juce::AudioBuffer<float>& buffer);

    void computeFFT(const juce::AudioBuffer<float>& buffer,
                    const std::vector<float>& windowIn,
                    std::vector<float>& fftDataTemp,
                    std::array<std::vector<Complex>, 2>& spectraOut);
    void computeCQT(const std::array<std::vector<Complex>, 2>& ffts,
                    const std::vector<std::vector<Complex>>& cqtKernelsIn,
                    std::array<std::vector<std::vector<std::complex<float>>>, 2>& spectraOut,
                    std::array<std::vector<float>, 2>& magnitudesOut);
    void computeILDs(const std::array<std::vector<float>, 2>& magnitudesIn,
                     std::vector<float>& panOut);
    void computeITDs(const std::array<std::vector<std::vector<std::complex<float>>>, 2>& spec,
                     int numBands,
                     std::vector<float>& panOut);

    float alphaForFreq(float f);
    float coherenceThresholdForFreq(float f);

    //=========================================================================
    /* Parameters - set from outside */

    int samplesPerBlock;
    double sampleRate;

    enum Transform transform = CQT;
    enum PanMethod panMethod = level_pan;
    enum FrequencyWeighting freqWeighting = A_weighting;
    
    int numCQTbins;
    float minCQTfreq; // Minimum CQT frequency in Hz
    float maxAmplitude; // Maximum expected (linear) amplitude of input signal
    float threshold; // dB relative to maxAmplitude

    //=========================================================================
    /* Block-size-dependent constants, calculated in prepareToPlay() */

    int fftSize; // Total number of FFT bins
    int numFFTBins; // Number of useful bins from FFT
    int numBands; // Number of frequency bands used from the selected transform
    float fftScaleFactor; // Scale factor to normalize FFT output
    float cqtScaleFactor; // Scale factor to normalize CQT output

    //=========================================================================
    /* Pre-allocated storage, etc. */

    std::unique_ptr<juce::dsp::FFT> fftEngine; // Calculates FFTs of length fftSize
    
    std::array<std::vector<Complex>, 2> fftSpectra; // For storing FFT results
    std::vector<float> fftData; // Temporary storage for in-place real fft
    std::array<std::vector<float>, 2> magnitudes;
    std::vector<float> binFrequencies; // Center freqs of CQT or FFT bins
    std::vector<float> ilds;
    std::vector<float> itds;
    std::vector<float> panIndices;
    std::vector<float> window; // Hann window of length fftSize
    std::vector<float> frequencyWeights; // Weighting factors for each freq bin
    std::vector<float> itdPerBin;
    std::vector<float> maxITD; // Max ITD per frequency band
    std::vector<frequency_band> newResults; // For storage before writing to the mutex-protected vector

    // Each filter is a complex-valued kernel vector (frequency domain)
    std::vector<std::vector<Complex>> cqtKernels;
    std::array<std::vector<std::vector<Complex>>, 2> fullCQTspec;
    
    // Frequency-dependent ITD/ILD parameters
    std::vector<float> itdWeights;
    std::vector<float> ildWeights;

    //=========================================================================
    std::vector<frequency_band> results; // Must be sorted by frequency!!!
    mutable std::mutex resultsMutex;

    std::unique_ptr<AnalyzerWorker> worker;

    // Atomic flag to indicate if the analyzer is prepared
    std::atomic<bool> isPrepared { false };
    mutable std::mutex prepareMutex;

    //=========================================================================
    /* Compile-time constants */

    static constexpr float epsilon = 1e-12f;
    static constexpr float pi = MathConstants<float>::pi;
    static constexpr float cqtNormalization = 1.0f / 28.0f;
    static constexpr float maxITDlow = 0.00066f; // Max ITD at lowest freq
    static constexpr float maxITDhigh = 0.0008f; // Max ITD at highest freq
    static constexpr float f_trans = 2000.0f; // ITD/ILD transition frequency
    static constexpr float p = 2.5f; // Slope
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

        // DBG("Enqueued block on audio thread.");
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
                // If there is no data ready, wait briefly or until notified
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