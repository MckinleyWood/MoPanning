/*=============================================================================

    This file is part of the MoPanning audio visuaization tool.
    Copyright (C) 2025 Owen Ohlson and Mckinley Wood

    This program is free software: you can redistribute it and/or modify 
    it under the terms of the GNU Affero General Public License as 
    published by the Free Software Foundation, either version 3 of the 
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but 
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
    Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public 
    License along with this program. If not, see 
    <https://www.gnu.org/licenses/>.

=============================================================================*/

/*  AudioAnalyzer.h

This file defines the AudioAnalyzer class, which is responsible for 
analyzing audio data using FFT and CQT transforms. It can compute 
frequency bands, magnitudes, and pan indices based on the audio input. 
The class also manages a worker thread for processing audio blocks 
asynchronously.
*/

#pragma once
#include <JuceHeader.h>
#include "Utils.h"

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
    void prepare(double newSampleRate, int newNumTracks);
    void prepare(); // Uses current sampleRate

    void setResultsPointer(std::array<TrackSlot, 8>* resultsPtr);

    // Called by audio thread
    void enqueueBlock(const juce::AudioBuffer<float>* buffer, int trackIndex);

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

    void stopWorker(std::unique_ptr<AnalyzerWorker>& worker);
    
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

    void analyzeBlock(const juce::AudioBuffer<float>& buffer, 
                    int trackIndex, 
                    std::vector<FrequencyBand>& outResults,
                    juce::dsp::FFT& fftEngine,
                    std::vector<float>& fftDataTemp,
                    std::array<std::vector<std::complex<float>>, 2>& outSpectra,
                    std::vector<std::complex<float>>& crossSpectrum,
                    std::vector<std::complex<float>>& crossCorr,
                    std::array<std::vector<float>, 2> magnitudes,
                    std::vector<float> ilds,
                    std::vector<float> itds,
                    std::vector<float> panIndices,
                    std::array<std::vector<std::vector<std::complex<float>>>, 2> fullCQTspec);

    void computeFFT(const juce::AudioBuffer<float>& buffer,
                    const std::vector<float>& windowIn,
                    std::vector<float>& fftDataTemp,
                    std::array<std::vector<Complex>, 2>& spectraOut,
                    juce::dsp::FFT& fftEngine);
    void computeCQT(const std::array<std::vector<Complex>, 2>& ffts,
                    const std::vector<std::vector<Complex>>& cqtKernelsIn,
                    std::array<std::vector<std::vector<std::complex<float>>>, 2>& spectraOut,
                    std::array<std::vector<float>, 2>& magnitudesOut);
    void computeILDs(const std::array<std::vector<float>, 2>& magnitudesIn,
                     std::vector<float>& panOut);
    void computeITDs(const std::array<std::vector<std::vector<std::complex<float>>>, 2>& spec,
                     int numBands,
                     std::vector<float>& panOut,
                     juce::dsp::FFT& fftEngine,
                     std::vector<std::complex<float>>& crossSpectrum,
                     std::vector<std::complex<float>>& crossCorr);
    float coherenceThresholdForFreq(float f);

    //=========================================================================
    /* Parameters - set from outside */

    int samplesPerBlock;
    double sampleRate;

    int numTracks;

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

    std::vector<float> binFrequencies; // Center freqs of CQT or FFT bins
    std::vector<float> window; // Hann window of length windowSize
    std::vector<float> frequencyWeights; // Weighting factors for each freq bin
    std::vector<float> itdPerBin;
    std::vector<float> maxITD; // Max ITD per frequency band

    // Each filter is a complex-valued kernel vector (frequency domain)
    std::vector<std::vector<Complex>> cqtKernels;
    
    // Frequency-dependent ITD/ILD parameters
    std::vector<float> itdWeights;
    std::vector<float> ildWeights;

    //=========================================================================
    std::array<TrackSlot, 8>* results;

    std::vector<std::unique_ptr<AnalyzerWorker>> workers; // One worker per track

    // Atomic flag to indicate if the analyzer is prepared or preparing
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
    AnalyzerWorker(int windowSizeIn, int hopSizeIn, double sampleRateIn, int numBandsIn, int trackIndexIn, AudioAnalyzer& parent) 
        : windowSize(windowSizeIn),
          hopSize(hopSizeIn),
          sampleRate(sampleRateIn),
          numBands(numBandsIn),
          trackIndex(trackIndexIn),
          parentAnalyzer(parent)
    {
        newResults.reserve(numBands);

        // Pre-allocate ring buffer - large enough for 16 windows or 2 seconds
        int bufferSize = std::max((int)sampleRate * 2, windowSize * 16);
        ringBuffer.setSize(2, bufferSize);
        
        // Pre-allocate analysis buffer
        analysisBuffer.setSize(2, windowSize);
        writePosition = 0;

        // Initialize per-worker FFT
        int order = static_cast<int>(std::log2(windowSize));
        fft = std::make_unique<juce::dsp::FFT>(order);

        // Pre-allocate scratch buffers
        fftDataTemp.assign(windowSize * 2, 0.0f);
        crossSpectrum.assign(windowSize, std::complex<float>(0.0f, 0.0f));
        crossCorr.assign(windowSize, std::complex<float>(0.0f, 0.0f));
        for (auto& spec : outSpectra)
            spec.assign(windowSize, std::complex<float>(0.0f, 0.0f));

        // Resize per-track analysis arrays
        magnitudes[0].assign(numBands, 0.0f);
        magnitudes[1].assign(numBands, 0.0f);
        ilds.assign(numBands, 0.0f);
        itds.assign(numBands, 0.0f);
        panIndices.assign(numBands, 0.0f);
        for (int ch = 0; ch < 2; ++ch)
        {
            fullCQTspec[ch].resize(numBands); // Resize vector inside array

            for (int bin = 0; bin < numBands; ++bin)
            {
                fullCQTspec[ch][bin].resize(windowSize);
            }
        }
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
        // DBG("Started AnalyzerWorker thread for track " << trackIndex);
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

        // DBG("PUSH Track " << trackIndex 
        //     << " L[0]=" << newBlock.getSample(0, 0) 
        //     << " R[0]=" << newBlock.getSample(1, 0)
        //     << " RMS L=" << newBlock.getRMSLevel(0, 0, n)
        //     << " R=" << newBlock.getRMSLevel(1, 0, n));

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

            // DBG("RUN() Track " << trackIndex 
            //     << " L[0]=" << analysisBuffer.getSample(0, 0) 
            //     << " R[0]=" << analysisBuffer.getSample(1, 0)
            //     << " RMS L=" << analysisBuffer.getRMSLevel(0, 0, n)
            //     << " R=" << analysisBuffer.getRMSLevel(1, 0, n));

            // Update the read position, wrapping around if necessary
            readPosition = (readPosition + hopSize) % N;

            // Pass the analysis buffer to the audio analyzer
            parentAnalyzer.analyzeBlock(analysisBuffer, 
                                        trackIndex, 
                                        newResults, 
                                        *fft,
                                        fftDataTemp,
                                        outSpectra,
                                        crossSpectrum,
                                        crossCorr,
                                        magnitudes,
                                        ilds,
                                        itds,
                                        panIndices,
                                        fullCQTspec);
        }
    }

    juce::AudioBuffer<float> ringBuffer;
    std::atomic<int> writePosition;
    int readPosition = 0;

    juce::AudioBuffer<float> analysisBuffer;
    int windowSize;
    int hopSize; 
    double sampleRate;
    int numBands;

    bool overloaded = false;

    int trackIndex;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftDataTemp;
    std::vector<std::complex<float>> crossSpectrum;
    std::vector<std::complex<float>> crossCorr;
    std::array<std::vector<std::complex<float>>, 2> outSpectra;
    std::array<std::vector<float>, 2> magnitudes;
    std::vector<float> ilds, itds, panIndices;
    std::array<std::vector<std::vector<std::complex<float>>>, 2> fullCQTspec;

    std::thread thread;
    std::atomic<bool> shouldExit {false};
    std::condition_variable cv;
    std::mutex mutex;

    std::vector<FrequencyBand> newResults;

    AudioAnalyzer& parentAnalyzer;
};