#include "AudioAnalyzer.h"
#include <cmath>

#pragma mark - Helper Functions

// Convert frequency to Mel scale
// static float hzToMel(float hz) noexcept
// {
//     return 2595.0f * std::log10(1.0f + hz / 700.0f);
// }

// // Convert Mel scale to frequency
// static float melToHz(float mel) noexcept
// {
//     return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
// }


// Set FFT size and allocate buffers
AudioAnalyzer::AudioAnalyzer(int fftOrderIn, float minCQTfreqIn, int binsPerOctaveIn)
    : fftOrder(11), // 2048 FFT size
      fftSize(1 << fftOrderIn),
      minCQTfreq(minCQTfreqIn),
      binsPerOctave(binsPerOctaveIn)
    //   numMelBands(40)
{
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    // Allocate buffers
    window.resize(fftSize);
    fftData.resize(fftSize);
    // magnitudeBuffer.resize(fftSize / 2 + 1); // Magnitudes for positive frequencies
}

// Set sample rate and block size, prepare FFT window and CQT kernels
void AudioAnalyzer::prepare(double sr, int blkSize)
{
    const std::lock_guard<std::mutex> lock(bufferMutex);

    sampleRate = sr;
    blockSize = blkSize;

    // Hann window
    for (int n = 0; n< fftSize; ++n)
    {
        window[n] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * n / (fftSize - 1)));
    }

    // CQT bins
    int octaves = std::floor(std::log2(sampleRate * 0.5f / minCQTfreq));
    numCQTbins = octaves * binsPerOctave;

    cqtKernels.clear();
    cqtKernels.resize(numCQTbins);

    // Precompute CQT kernels
    for (int bin = 0; bin < numCQTbins; ++bin)
    {
        // Compute center frequency for this bin
        float freq = minCQTfreq * std::pow(2.0f, static_cast<float>(bin) / binsPerOctave);
        int kernelLength = fftSize;

        std::vector<std::complex<float>> kernelTime(kernelLength, 0.0f);


        // Generate complex sinusoid for this frequency
        for (int n = 0; n < kernelLength; ++n)
        {
            float t = static_cast<float>(n) / sampleRate;
            float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * n / (kernelLength - 1)));
            kernelTime[n] = std::polar(w, -2.0f * juce::MathConstants<float>::pi * freq * t); // complex sinusioid * window
        }

        // Zero-pad and FFT
        std::vector<std::complex<float>> kernelFreq(kernelLength);
        juce::dsp::FFT kernelFFT(static_cast<int>(std::log2(kernelLength)));
        kernelFFT.perform(kernelTime.data(), kernelFreq.data(), false);

        cqtKernels[bin] = std::move(kernelFreq);
    }

    // Allocate CQT result: 2 channels default
    cqtMagnitudes.resize(2, std::vector<float>(numCQTbins, 0.0f));


    // USE LATER
    // Mel filterbank
    // createMelFilterbank();

    // Initialize melSpectrum with stereo by default
    // melSpectrum.setSize(2, numMelBands);
}

void AudioAnalyzer::analyzeBlock(const juce::AudioBuffer<float>& buffer)
{
    const std::lock_guard<std::mutex> lock(bufferMutex);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    if (analysisBuffer.getNumChannels() != numChannels ||
        analysisBuffer.getNumSamples() != numSamples)
    {
        analysisBuffer.setSize(numChannels, numSamples);
    }

    // Copy input buffer to analysis buffer
    for (int ch = 0; ch < numChannels; ++ch)
    {
        analysisBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }

    // Compute GCC-PHAT ITD (Interaural Time Difference)
    if (numChannels >= 2)
    {
    const float* left = analysisBuffer.getReadPointer(0);
    const float* right = analysisBuffer.getReadPointer(1);

    float itd = computeGCCPHAT_ITD(left, right, numSamples);
    itdEstimates.push_back(itd); // Optional: keep history
    }

    // Resize CQT magnitudes if channel count changed
    if (static_cast<int>(cqtMagnitudes.size()) != numChannels)
        cqtMagnitudes.resize(numChannels, std::vector<float>(numCQTbins, 0.0f));

    // Compute CQT for each channel
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* channelData = analysisBuffer.getReadPointer(ch);
        computeCQT(channelData, ch);  // populate cqtMagnitudes[ch]
    }

    // === Panning Index Calculation (based on Avendano) ===
    
    const int numBins = numCQTbins;
    panningSpectrum.setSize(1, numBins);
    panningSpectrum.clear();

    // Only compute panning spectrum if stereo
    if (numChannels == 2)
    {
        // Compute panning spectrum as average of CQT magnitudes
        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto& left = cqtMagnitudes[0];
            const auto& right = cqtMagnitudes[1];
            float* panningData = panningSpectrum.getWritePointer(0);

            for (int k = 0; k < numBins; ++k)
            {
                const float L = left[k];
                const float R = right[k];

                const float denom = (L * L + R * R);
                if (denom < 1e-6f)
                {
                    panningData[k] = 0.0f; // Avoid division by zero
                    continue;
                }
                
                const float cross = 2.0f * L * R;
                const float similarity = cross / denom;

                // Compute directional component Δ(m,k)
                const float psi1 = (L * R) / (L * L + 1e-6f);
                const float psi2 = (L * R) / (R * R + 1e-6f);
                const float delta = psi1 - psi2;

                float direction = 0.0f;
                if (delta > 0) direction = 1.0f;
                else if (delta < 0) direction = -1.0f;

                panningData[k] = (1.0f - similarity) * direction;
            }
        }
    }

    // MORE MEL STUFF
    // // Resize melSpectrum if channel count changed
    // if (melSpectrum.getNumChannels() != numChannels ||
    //     melSpectrum.getNumSamples() != numMelBands)
    // {
    //     melSpectrum.setSize(numChannels, numMelBands);
    // }

    // // For each channel: compute FFT -> magnitudeBuffer -> apply Mel filters
    // for (int ch = 0; ch < numChannels; ++ch)
    // {
    //     const float* channelData = analysisBuffer.getReadPointer(ch);
    //     computeFFT(channelData);

    //     // magnitudeBuffer now holds fftSize/2 + 1 magnitudes
    //     for (int m = 0; m < numMelBands; ++m)
    //     {
    //         float melEnergy = 0.0f;
    //         for (int b = 0; b < static_cast<int>(magnitudeBuffer.size()); ++b)
    //             melEnergy += magnitudeBuffer[b] * melFilters[m][b];

    //         // Store raw Mel energy (you can log-scale or normalize later)
    //         melSpectrum.setSample(ch, m, melEnergy);
    //     }
    // }
}


// Possibly later on split this into "computeFFT" and "applyCQT" if we need just FFT for other calculations
void AudioAnalyzer::computeCQT(const float* channelData, int channelIndex)
{
    // Load windowed samples (real part) and zero-pad imag part
    for (int n = 0; n < fftSize; ++n)
    {
        float x = (n < blockSize) ? channelData[n] : 0.0f;
        fftData[n] = std::complex<float>(x * window[n], 0.0f);
    }

    // In-place complex FFT (real -> complex)
    // Data layout: [ real[0], real[1], ..., real[fftSize-1], imag[0], imag[1], ..., imag[fftSize-1] ]
    fft->perform(fftData.data(), fftData.data(), false);

    // Compute CQT by inner product with each kernel
    for (int bin = 0; bin < numCQTbins; ++bin)
    {
        std::complex<float> sum = 0.0f;
        const auto& kernel = cqtKernels[bin];
        for (int i = 0; i < fftSize; ++i)
            sum += fftData[i] * std::conj(kernel[i]);
        
        cqtMagnitudes[channelIndex][bin] = std::abs(sum);
    }
}

// void AudioAnalyzer::createMelFilterbank()
// {
//     int numFftBins = fftSize / 2 + 1;

//     // Mel frequency range: 0 to Nyquist
//     float melMin = hzToMel(0.0f);
//     float melMax = hzToMel(static_cast<float>(sampleRate / 2.0));

//     // Compute (numMelBands + 2) Mel points uniformly spaced
//     std::vector<float> melPoints(numMelBands + 2);
//     for (int i = 0; i < numMelBands + 2; ++i)
//     {
//         melPoints[i] = melMin + ((melMax - melMin) * i) / (numMelBands + 1); 
//     }

//     // Convert Mel points to Hz
//     std::vector<float> freqPoints(numMelBands + 2);
//     for (int i = 0; i < numMelBands + 2; ++i)
//         freqPoints[i] = melToHz(melPoints[i]);

//     // Convert Hz to nearest FFT bin index
//     std::vector<int> binPoints(numMelBands + 2);
//     for (int i = 0; i < numMelBands + 2; ++i)
//     {
//         float f = freqPoints[i];
//         int bin = static_cast<int>(std::floor((fftSize + 1) * f / sampleRate));
//         if (bin < 0) bin = 0;
//         if (bin >= numFftBins) bin = numFftBins - 1;
//         binPoints[i] = bin;
//     }

//     // Allocate melFilters: [40 bands] * [numFftBins]
//     melFilters.clear();
//     melFilters.resize(numMelBands, std::vector<float>(numFftBins, 0.0f));

//     // Build triangular filters
//     for (int m = 0; m < numMelBands; ++m)
//     {
//         int leftBin = binPoints[m];
//         int centerBin = binPoints[m + 1];
//         int rightBin = binPoints[m + 2];

//         // Rising slope (left bin to center bin)
//         for (int b = leftBin; b < centerBin; ++b)
//         {
//             float weight = (static_cast<float>(b) - leftBin) / (float)(centerBin - leftBin);
//             melFilters[m][b] = weight;
//         }

//         // Falling slope (center bin to right bin)
//         for (int b = centerBin; b < rightBin; ++b)
//         {
//             float weight = (float)(rightBin - b) / (float)(rightBin - centerBin);
//             melFilters[m][b] = weight;
//         }
//     }
// }

// juce::AudioBuffer<float> AudioAnalyzer::getMelSpectrum() const
// {
//     const std::lock_guard<std::mutex> lock(bufferMutex);
//     return melSpectrum;
// }

