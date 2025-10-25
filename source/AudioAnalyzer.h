/*  AudioAnalyzer.h

This file defines the AudioAnalyzer class, which is responsible for 
analyzing audio data using FFT and CQT transforms. It can compute 
frequency bands, magnitudes, and pan indices based on the audio input. 
The class also manages a worker thread for processing audio blocks 
asynchronously.
*/

#pragma once
#include <JuceHeader.h>


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
    void prepare(double newSampleRate);
    void prepare(); // Uses current sampleRate

    // Called by audio thread
    void enqueueBlock(const juce::AudioBuffer<float>* buffer);

    // Called by GUI thread to get latest results
    std::vector<frequency_band> getLatestResults() const;

    void setWindowSize(int newWindowSize);
    void setHopSize(int newHopSize);
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

    void setScaleFactors(int windowSizeIn, 
                         float maxAmplitudeIn, 
                         float cqtNormalizationIn);
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

    int windowSize; // Total number of FFT bins
    int hopSize; // Number of samples between analysis windows
    int numFFTBins; // Number of useful bins from FFT
    int numBands; // Number of frequency bands used from the selected transform
    float fftScaleFactor; // Scale factor to normalize FFT output
    float cqtScaleFactor; // Scale factor to normalize CQT output

    //=========================================================================
    /* Pre-allocated storage, etc. */

    std::unique_ptr<juce::dsp::FFT> fftEngine; // Calculates FFTs of length windowSize
    
    std::array<std::vector<Complex>, 2> fftSpectra; // For storing FFT results
    std::vector<float> fftData; // Temporary storage for in-place real fft
    std::array<std::vector<float>, 2> magnitudes;
    std::vector<float> binFrequencies; // Center freqs of CQT or FFT bins
    std::vector<float> ilds;
    std::vector<float> itds;
    std::vector<float> panIndices;
    std::vector<float> window; // Hann window of length windowSize
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
    AnalyzerWorker(int windowSizeIn, int hopSizeIn, double sampleRateIn, AudioAnalyzer& parent) 
        : windowSize(windowSizeIn),
          hopSize(hopSizeIn),
          sampleRate(sampleRateIn),
          parentAnalyzer(parent)
    {
        // Pre-allocate ring buffer - large enough for 16 windows or 2 seconds
        int bufferSize = std::max((int)sampleRate * 2, windowSize * 16);
        ringBuffer.setSize(2, bufferSize);
        
        // Pre-allocate analysis buffer
        analysisBuffer.setSize(2, windowSize);

        writePosition = 0;
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

    void setHopSize(int newHopSize)
    {
        hopSize = newHopSize;
    }

    /*  This function is called on audio thread to enqueue a copy of 
        the incoming audio block into the ring buffer. 
    */
    void pushBlock(const juce::AudioBuffer<float>& newBlock)
    {
        int n = newBlock.getNumSamples();
        int N = ringBuffer.getNumSamples();

        for (int ch = 0; ch < 2; ++ch)
        {
            if (writePosition + n < N)
            {
                // We have room, copy the samples starting from writePosition
                ringBuffer.copyFrom(ch, writePosition, newBlock, ch, 0, n);
            }
            else
            {
                int samplesToEnd = N - writePosition;
                // Copy the samples from writePosition to end of the ring buffer
                ringBuffer.copyFrom(ch, writePosition, newBlock, ch, 0, samplesToEnd);

                // Copy the rest of the samples from the start of the ring buffer
                ringBuffer.copyFrom(ch, 0, newBlock, ch, samplesToEnd, n - samplesToEnd);
            }
        }
        
        // Update the write position, wrapping around if necessary
        writePosition = (writePosition + n) % N;

        // Notify worker thread that new data is available
        std::lock_guard<std::mutex> lock(mutex);
        cv.notify_one();

        // DBG("Enqueued block on audio thread.");
    }

private:
    /*  This function is run on a background thread to continuously
        check if there are enough samples available to run analysis, and
        to copy them to the audio analyzer if so. 
    */
    void run()
    {
        while (!shouldExit)
        {
            int n = windowSize;
            int N = ringBuffer.getNumSamples();

            int samplesAvailable = (writePosition - readPosition + N) % N;
            
            // Check if there is data ready
            if (samplesAvailable < windowSize)
            {
                // If there is no data ready, wait briefly or until notified
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait_for(lock, std::chrono::milliseconds(2));
                continue; // Re-check condition
            }

            // DBG("Worker thread processing block.");

            // Check if we are running behind the audio thread
            if (samplesAvailable > windowSize * 8)
            {
                // If we are too far behind, skip ahead to the latest data
                readPosition = (writePosition - windowSize * 2 + N) % N;
                overloaded = true;
                // DBG("AnalyzerWorker overloaded, skipping ahead.");
            }

            // Copy data from the ring buffer to the analysis buffer
            for (int ch = 0; ch < 2; ++ch)
            {
                if (readPosition + n <= N)
                {
                    // We have room, copy the samples starting from readPosition
                    analysisBuffer.copyFrom(ch, 0, ringBuffer, ch, readPosition, n);
                }
                else
                {
                    int samplesToEnd = N - readPosition;
                    // Copy the samples from readPosition to end of the ring buffer
                    analysisBuffer.copyFrom(ch, 0, ringBuffer, ch, readPosition, samplesToEnd);

                    // Copy the rest of the samples from the start of the ring buffer
                    analysisBuffer.copyFrom(ch, samplesToEnd, ringBuffer, ch, 0, n - samplesToEnd);
                }
            }

            // Update the read position, wrapping around if necessary
            readPosition = (readPosition + hopSize) % N;

            // Pass the analysis buffer to the audio analyzer
            parentAnalyzer.analyzeBlock(analysisBuffer);
        }
    }

    juce::AudioBuffer<float> ringBuffer;
    std::atomic<int> writePosition;
    int readPosition = 0;

    juce::AudioBuffer<float> analysisBuffer;
    int windowSize;
    int hopSize; 
    double sampleRate;

    bool overloaded = false;

    std::thread thread;
    std::atomic<bool> shouldExit {false};
    std::condition_variable cv;
    std::mutex mutex;

    AudioAnalyzer& parentAnalyzer;
};