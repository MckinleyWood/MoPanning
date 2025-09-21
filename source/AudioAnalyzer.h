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
    void prepareToPlay(int samplesPerBlock, double sampleRate);
    void prepareToPlay(); // Uses current sampleRate and samplesPerBlock

    // Called by audio thread
    void enqueueBlock(const juce::AudioBuffer<float>* buffer);

    // Called by GUI thread to get latest results
    std::vector<frequency_band> getLatestResults() const;

    void setTransform(Transform newTransform);
    void setPanMethod(PanMethod newPanMethod);
    void setFFTOrder(int newFFTOrder);
    void setNumCQTBins(int newNumCQTBins);
    void setMinFrequency(float newMinFrequency);
    void setMaxAmplitude(float newMaxAmplitude);
    void setThreshold(float newThreshold);
    void setAWeighting(float newAWeighting);

    bool getPrepared() const { return isPrepared.load(); }
    void setPrepared(bool prepared) { isPrepared.store(prepared); }

    //=========================================================================
    class AnalyzerWorker;

    void stopWorker();
    
private:
    //=========================================================================
    /* Setup functions */

    void setupAWeights(const std::vector<float>& freqs,
                       std::vector<float>& weights);
    void setupCQT();
    
    /* Analysis functions */

    void computeFFT(const juce::AudioBuffer<float>& buffer,
                    std::array<fft_buffer_t, 2>& outSpectra);
    void computeILDs(std::array<std::vector<float>, 2>& magnitudes,
                     int numBands, std::vector<float>& outPan);
    void computeCQT(const juce::AudioBuffer<float>& buffer,
                    std::array<fft_buffer_t, 2> ffts,
                    std::array<std::vector<float>, 2>& cqtMags);
    void computeITDs(std::vector<std::vector<std::vector<std::complex<float>>>> spec,
                    const std::array<std::vector<float>, 2>& cqtMags,
                    int numBands,
                    std::vector<float>& panIndices);
    void analyzeBlock(const juce::AudioBuffer<float>& buffer);

    //=========================================================================
    /* Parameters */

    int samplesPerBlock;
    double sampleRate;

    enum Transform transform;
    enum PanMethod panMethod;
    
    int fftOrder; // FFT order = log2(fftSize) - not used at the moment
    int numCQTbins;
    float minCQTfreq; // Minimum CQT frequency in Hz
    float maxAmplitude; // Maximum expected (linear) amplitude of input signal
    float threshold; // dB relative to maxAmplitude

    //=========================================================================

    std::vector<float> window; // Hann window of length fftSize

    int fftSize;
    std::unique_ptr<juce::dsp::FFT> fft; // JUCE FFT engine
    fft_buffer_t fftBuffer;
    int numFFTBins; // Number of useful bins from FFT
    float fftScaleFactor; // Scale factor to normalize FFT output
    float cqtScaleFactor; // Scale factor to normalize CQT output
    std::vector<float> centerFrequencies; // Center freqs of CQT or FFT bins
    std::vector<float> aWeights; // A-weighting factors for each freq bin
    bool AWeighting; // Whether to apply A-weighting

    // Each filter is a complex-valued kernel vector (frequency domain)
    std::vector<std::vector<std::complex<float>>> cqtKernels;

    std::vector<std::vector<std::vector<std::complex<float>>>> fullCQTspec;
    
    // Frequency-dependent ITD/ILD parameters
    std::vector<float> maxITD; // max ITD per frequency band
    float maxITDlow = 0.00066f; // max ITD at lowest freq
    float maxITDhigh = 0.0008f; // max ITD at highest freq
    float f_trans = 2000.0f; // ITD/ILD transition frequency
    float p  = 2.5f;    // slope
    std::vector<float> itdWeights;
    std::vector<float> ildWeights;
    float alphaForFreq(float f)
    {
        // log-scale (Hz)
        float logf = std::log10(f + 1.0f);   // +1 to avoid log(0)
        float logLow = std::log10(100.0f);   // anchor: 100 Hz
        float logHigh = std::log10(4000.0f); // anchor: 4 kHz

        // Normalize [0..1] across range
        float t = juce::jlimit(0.0f, 1.0f, (logf - logLow) / (logHigh - logLow));

        // Interpolate smoothly between 0.8 and 1.0
        return 0.8f + t * (1.0f - 0.4f);
    }

    // float coherenceThresholdForFreq(float f)
    // {
    //     float logf = std::log10(f + 1.0f);
    //     float logLow = std::log10(100.0f);
    //     float logHigh = std::log10(4000.0f);

    //     float t = juce::jlimit(0.0f, 1.0f, (logf - logLow) / (logHigh - logLow));

    //     // Threshold from ~1e-7 at lows → ~1e-6 at highs
    //     float lowThresh = 1e-7f;
    //     float highThresh = 1e-6f;

    //     return lowThresh * std::pow((highThresh / lowThresh), t); // geometric interpolation
    // }

    // /* Compute expansion factor for a given frequency bin, taking 
    // into account low-frequency dominance and FFT size */
    // float itdExpansionForFreq(float freq, 
    //                           const std::array<std::vector<float>, 2>& cqtMags, 
    //                           int bin)
    // {
    //     // Parameters
    //     const float maxExpansion = 1.5f;      // Max ITD boost for low-frequency dominant signals
    //     const float lowFreqCutoff = 200.0f;   // Hz considered "low"
    //     const float referenceFFT = 1024.0f;   // FFT size that looks "perfect"
    //     const float minFFTBoost = 1.0f;       // No boost at referenceFFT
    //     const float eps = 1e-12f;

    //     // Compute low-frequency energy ratio
    //     float leftLF = 0.0f, rightLF = 0.0f;
    //     float totalLeft = 0.0f, totalRight = 0.0f;

    //     for (int b = 0; b < numCQTbins; ++b)
    //     {
    //         float f = centerFrequencies[b];
    //         float magL = cqtMags[0][b];
    //         float magR = cqtMags[1][b];

    //         totalLeft  += magL;
    //         totalRight += magR;

    //         if (f <= lowFreqCutoff)
    //         {
    //             leftLF  += magL;
    //             rightLF += magR;
    //         }
    //     }

    //     float lfRatio = ((leftLF + rightLF) / (totalLeft + totalRight + eps));

    //     // Base low-frequency expansion
    //     float baseFactor = 1.0f;
    //     float lfExpansion = (freq <= lowFreqCutoff) 
    //                         ? baseFactor + (maxExpansion - baseFactor) * lfRatio
    //                         : 1.0f;

    //     // FFT-size-dependent boost: smaller FFT → slightly larger expansion
    //     float fftBoost = 1.0f + (referenceFFT - fftSize) / referenceFFT * 0.2f; 
    //     fftBoost = juce::jlimit(1.0f, 1.2f, fftBoost); // clamp boost to reasonable range

    //     return lfExpansion * fftBoost;
    // }

    std::vector<frequency_band> results; // Must be sorted by frequency!!!
    mutable std::mutex resultsMutex;

    // Worker 
    std::unique_ptr<AnalyzerWorker> worker;

    // Atomic flag to indicate if the analyzer is prepared
    std::atomic<bool> isPrepared { false };
    mutable std::mutex prepareMutex;
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