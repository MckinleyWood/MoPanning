#pragma once

#include <JuceHeader.h>
#include <mutex>
#include <vector>

/*
    AudioAnalyzer...

*/

struct frequency_band {
    float frequency; // Band frequency in Hertz
    float amplitude; // Range 0 - 1 ...?
    float pan_index; // -1 = far left, +1 = far right
};

typedef std::vector<juce::dsp::Complex<float>> fft_buffer_t;

class AudioAnalyzer
{
public:
    //=========================================================================
    AudioAnalyzer();
    ~AudioAnalyzer();

    // Must be called before analyzeBlock()
    void prepare(int samplesPerBlock, double sampleRate);

    // Called by audio thread
    void enqueueBlock(const juce::AudioBuffer<float>* buffer);

    // Called by GUI thread to get latest results
    std::vector<frequency_band> getLatestResults() const;

    // These should be called whenever the sample rate / block size changes
    void setSamplesPerBlock(int newSamplesPerBlock);
    void setSampleRate(int newSampleRate);

    //=========================================================================
    class AnalyzerWorker;
    
private:
    //=========================================================================
    // Analysis functions
    void computeFFT(const juce::AudioBuffer<float>& buffer,
                    std::array<fft_buffer_t, 2>& outSpectra);
    void computeILDs(const std::vector<float>& magsL,
                     const std::vector<float>& magsR,
                     std::vector<float>& outPan);
    void analyzeBlock(const juce::AudioBuffer<float>& buffer);

    //=========================================================================
    int samplesPerBlock;
    double sampleRate;

    int fftOrder = 9; // FFT order = log2(fftSize)
    int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft{ fftOrder }; // JUCE FFT engine
    std::vector<float> window; // Hann window of length fftSize
    fft_buffer_t fftBuffer;
    int numBins = fftSize / 2 + 1;
    float maxExpectedMag;
    
    // Latest results stored for GUI
    std::vector<frequency_band> results;
    mutable std::mutex resultsMutex;

    // Worker 
    std::unique_ptr<AnalyzerWorker> worker;
};


//=============================================================================
class AudioAnalyzer::AnalyzerWorker
{
public:
    AnalyzerWorker(int numSlots, int blockSizeIn, AudioAnalyzer& parent)
        : fifo(numSlots), 
          buffers(static_cast<size_t>(numSlots)), 
          parentAnalyzer(parent)
    {
        // Pre-allocate buffers
        for (auto& buf : buffers)
            buf.setSize(2, blockSizeIn);

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
        buffers[static_cast<size_t>(start1)].makeCopyOf(input);
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
                    parentAnalyzer.analyzeBlock(
                        buffers[static_cast<size_t>(start1)]);
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
    std::atomic<bool> shouldExit {false};
    std::condition_variable cv;
    std::mutex mutex;

    AudioAnalyzer& parentAnalyzer;
    int blockSize;
};