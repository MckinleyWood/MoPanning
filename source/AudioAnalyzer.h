#pragma once

#include <JuceHeader.h>
#include <mutex>
#include <vector>
#include <juce_dsp/juce_dsp.h>

/*
    AudioAnalyzer

    - CQT
    - GCC-PHAT for inter-channel delay estimation
    - Panning spectrum based on CQT magnitudes
    - Thread-safe: locks when writing, returns copies for external reads

*/
class AudioAnalyzer
{
public:
    //=========================================================================
    /** 
        Constructor arguments:
          - fftOrderIn:  FFT size = 2^fftOrderIn (default 11 → 2048).
          - minCQTfreqIn: Lowest CQT center frequency in Hz (e.g. 20.0).
                          Must be > 0 and < Nyquist.
          - binsPerOctaveIn: Number of CQT bins per octave (e.g. 24 for quarter‐tones).
    */
    AudioAnalyzer(int fftOrderIn = 11, float minCQTfreqIn = 20.0f, int binsPerOctaveIn = 24);
    ~AudioAnalyzer();

    // Must be called before analyzeBlock()
    void prepare(int samplesPerBlock, double sampleRate);

    // Called by audio thread
    void enqueueBlock(const juce::AudioBuffer<float>* buffer);

    // Called by GUI thread to get latest results
    std::vector<std::vector<float>> getLatestCQTMagnitudes() const
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        return latestCQTMagnitudes;
    }
    std::vector<float> getLatestCombinedPanning() const
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        return latestCombinedPanning;
    }

    class AnalyzerWorker;

    /** 
        Returns thread-safe copy of per-channel CQT magnitudes:
        Dimensions: [numChannels] × [numCQTbins]
     * */ 
    // void analyzeBlock(const juce::AudioBuffer<float>* buffer);

private:
    //=========================================================================
    mutable std::mutex bufferMutex;
    juce::AudioBuffer<float> analysisBuffer; // Buffer for raw audio data

    //=========================================================================
    double sampleRate = 44100.0; //Defaults
    int samplesPerBlock = 512;
    
    int fftOrder;
    int fftSize;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    std::vector<juce::dsp::Complex<float>> fftData;
    std::vector<float> magnitudeBuffer;

    // === CQT ===
    void computeCQT(const float* channelData, int channelIndex);
    float minCQTfreq;
    int binsPerOctave;
    int numCQTbins;

    // Each filter is a complex-valued kernel vector (frequency domain)
    std::vector<std::vector<std::complex<float>>> cqtKernels;

    // CQT result: [numChannels][numCQTbins]
    std::vector<std::vector<float>> cqtMagnitudes;
    
    // === Panning ===
    juce::AudioBuffer<float> ILDpanningSpectrum;
    const juce::AudioBuffer<float>& getILDPanningSpectrum() const { return ILDpanningSpectrum; }
    std::vector<float> ITDpanningSpectrum;

    // === GCC-PHAT ===
    float computeGCCPHAT_ITD(const float* left, const float* right, int numSamples);

    std::vector<float> itdPerBand;

    float gccPhatDelayPerBand(const float* x, const float* y, int size, int fftOrder, 
                                        juce::dsp::IIR::Filter<float>& bandpassLeft, 
                                        juce::dsp::IIR::Filter<float>& bandpassRight);

    std::vector<juce::dsp::IIR::Filter<float>> leftBandpassFilters;
    std::vector<juce::dsp::IIR::Filter<float>> rightBandpassFilters;
    std::vector<float> centerFrequencies;

    void prepareBandpassFilters(double sampleRate);
    void computeGCCPHAT_ITD();

    float computeCQTCenterFrequency(int binIndex) const;

    //=========================================================================
    // Latest results stored for GUI
    std::vector<std::vector<float>> latestCQTMagnitudes;
    std::vector<float> latestCombinedPanning;
    mutable std::mutex resultsMutex;

    // Worker
    std::unique_ptr<AnalyzerWorker> worker;

    // Internal processing called by worker thread
    void analyzeBlock(const juce::AudioBuffer<float>& buffer);
};


//============================================================================
class AudioAnalyzer::AnalyzerWorker
{
public:
    AnalyzerWorker(int numSlots, 
                   int blockSize, 
                   int numChannels, 
                   AudioAnalyzer& parent)
        : fifo(numSlots), buffers(numSlots), parentAnalyzer(parent)
    {
        // Pre-allocate buffers
        for (auto& buf : buffers)
            buf.setSize(numChannels, blockSize);

        // Launch background thread
        shouldExit = false;
        thread = std::thread([this] { run(); });
    }

    ~AnalyzerWorker()
    {
        // Signal thread to exit
        shouldExit = true;
        cv.notify_one();
        if (thread.joinable())
            thread.join();
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
                if (size1 > 0)
                {
                    // Process this buffer
                    parentAnalyzer.analyzeBlock(buffers[start1]);
                    fifo.finishedRead(size1);
                }
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
    std::atomic<bool> shouldExit{false};
    std::condition_variable cv;
    std::mutex mutex;

    AudioAnalyzer& parentAnalyzer;
    int numChannels, blockSize;
};