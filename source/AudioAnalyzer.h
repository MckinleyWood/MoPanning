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
    /** 
        Constructor arguments:
          - fftOrderIn:  FFT size = 2^fftOrderIn (default 11 → 2048).
          - minCQTfreqIn: Lowest CQT center frequency in Hz (e.g. 20.0).
                          Must be > 0 and < Nyquist.
          - binsPerOctaveIn: Number of CQT bins per octave (e.g. 24 for quarter‐tones).
    */
    AudioAnalyzer(int fftOrderIn = 11, float minCQTfreqIn = 20.0f, int binsPerOctaveIn = 24);

    // Must be called before analyzeBlock()
    void prepare(int samplesPerBlock, double sampleRate);

    /** 
        Returns thread-safe copy of per-channel CQT magnitudes:
        Dimensions: [numChannels] × [numCQTbins]
     * */ 
    void analyzeBlock(const juce::AudioBuffer<float>* buffer);

private:
    mutable std::mutex bufferMutex;

    juce::AudioBuffer<float> analysisBuffer; // Buffer for raw audio data

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

};
