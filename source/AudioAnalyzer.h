#pragma once

#include <JuceHeader.h>
#include <mutex>
#include <vector>

/*
    AudioAnalyzer

    - Uses a 2048-point FFT (fftOrder = 11) per channel
    - CQT
    - Thread-safe: locks when writing, returns copies for external reads

    *FOR LATER*
    -  Applies a 40-band Mel filterbank to each magnitude spectrum
    - Stores per-channel Mel-band energies in melSpectrum (channels × 40)
*/

class AudioAnalyzer
{
public:
    /** 
        Constructor arguments:
          - fftOrderIn:  FFT size = 2^fftOrderIn (default 11 → 2048).
          - minCQTfreqIn: Lowest CQT center frequency in Hz (e.g. 20.0).
                          Must be > 0 and < Nyquist.
          - binsPerOctaveIn: Number of CQT bins per octave (e.g. 24 for quarter‐tones).
    */
    AudioAnalyzer(int fftOrderIn = 11, float minCQTfreqIn = 20.0f, int binsPerOctaveIn = 24);

    // Must be called before analyzeBlock()
    void prepare(double sampleRate, int blockSize);

    /** 
        Returns thread-safe copy of per-channel CQT magnitudes:
        Dimensions: [numChannels] × [numCQTbins]
     * */ 
    void analyzeBlock(const juce::AudioBuffer<float>& buffer);

    // juce::AudioBuffer<float> getMelSpectrum() const;

private:
    void computeCQT(const float* channelData, int channelIndex);

    // void createMelFilterbank();

    mutable std::mutex bufferMutex;

    juce::AudioBuffer<float> analysisBuffer; // Buffer for raw audio data

    double sampleRate = 44100.0;
    int blockSize = 512;
    
    int fftOrder;
    int fftSize;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    std::vector<juce::dsp::Complex<float>> fftData;
    std::vector<float> magnitudeBuffer;

    // === CQT Parameters ===
    float minCQTfreq;
    int binsPerOctave;
    int numCQTbins;

    // Each filter is a complex-valued kernel vector (frequency domain)
    std::vector<std::vector<std::complex<float>>> cqtKernels;

    // CQT result: [numChannels][numCQTbins]
    std::vector<std::vector<float>> cqtMagnitudes;
    
    // === Panning parameters ===
    juce::AudioBuffer<float> panningSpectrum;
    const juce::AudioBuffer<float>& getPanningSpectrum() const { return panningSpectrum; }


    // === GCC-PHAT parameters ===
    float computeGCCPHAT_ITD(const float* left, const float* right, int numSamples);
    std::vector<float> itdEstimates; // one per block or frequency band


    // === Mel filterbank parameters ===
    // int numMelBands;
    // std::vector<std::vector<float>> melFilters;
    // juce::AudioBuffer<float> melSpectrum;



};
