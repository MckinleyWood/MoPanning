#include "AudioAnalyzer.h"
#include <cmath>

//=============================================================================
// Set FFT size and allocate buffers
AudioAnalyzer::AudioAnalyzer()
{

}

AudioAnalyzer::~AudioAnalyzer()
{
    // Destroy worker, which joins thread
    worker.reset();
}

//=============================================================================
/*  Set sample rate and block size, prepare FFT window and CQT kernels,
    precompute CQT center frequencies, and prepare bandpass filters for 
    GCC-PHAT.
*/
void AudioAnalyzer::prepare(int fftOrderIn, int samplesPerBlockIn, double sampleRateIn, float minCQTfreqIn, 
                            int numCQTbinsIn)
{
    // Ensure thread safety
    const std::lock_guard<std::mutex> lock(bufferMutex);

    this->fftOrder = fftOrderIn;
    this->fftSize = 1 << fftOrder;
    this->samplesPerBlock = samplesPerBlockIn;
    this->sampleRate = sampleRateIn;
    this->minCQTfreq = minCQTfreqIn;
    this->numCQTbins = numCQTbinsIn;

    fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    // Allocate buffers
    window.resize(static_cast<size_t>(fftSize));
    fftData.resize(static_cast<size_t>(fftSize));

    // Hann window for CQT (no spectral leakage)
    for (size_t n = 0; n < static_cast<size_t>(fftSize); ++n)
    {
        float twoPi = 2.0f * juce::MathConstants<float>::pi;
        float angle = twoPi * static_cast<float>(n) / (fftSize - 1);
        float windowValue = 0.5f * (1.0f - std::cos(angle));
        window[n] = windowValue;
    }

    cqtKernels.clear();
    cqtKernels.resize(static_cast<size_t>(numCQTbins));
    centerFrequencies.resize(static_cast<size_t>(numCQTbins));

    // Set frequency from min to Nyquist
    const float nyquist = static_cast<float>(sampleRate * 0.5);
    const float logMin = std::log2(minCQTfreq);
    const float logMax = std::log2(nyquist);

    // Precompute CQT kernels
    for (size_t bin = 0; bin < static_cast<size_t>(numCQTbins); ++bin)
    {
        // Compute center frequency for this bin
        float frac = static_cast<float>(bin) / static_cast<float>(numCQTbins - 1);
        float freq = std::exp(logMin + frac * (logMax - logMin)); // Log-spaced
        centerFrequencies[bin] = freq;

        // Generate complex sinusoid for this frequency (for inner products)
        int kernelLength = fftSize;
        std::vector<std::complex<float>> kernelTime(static_cast<size_t>(kernelLength), 0.0f);
        for (size_t n = 0; n < static_cast<size_t>(kernelLength); ++n)
        {
            float t = static_cast<float>(n) / static_cast<float>(sampleRate);
            float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * n / (kernelLength - 1)));
            kernelTime[n] = std::polar(w, -2.0f * juce::MathConstants<float>::pi * freq * t); // complex sinusioid * window
        }

        // Zero-pad and FFT-- convert to frequency domain
        std::vector<std::complex<float>> kernelFreq(static_cast<size_t>(kernelLength));
        juce::dsp::FFT kernelFFT(static_cast<int>(std::log2(kernelLength)));
        kernelFFT.perform(kernelTime.data(), kernelFreq.data(), false);

        cqtKernels[bin] = std::move(kernelFreq);
    }

    // Allocate CQT result: 2 channels default
    cqtMagnitudes.resize(2, std::vector<float>(static_cast<size_t>(numCQTbins), 0.0f));

    // Prepare bandpass filters for GCC-PHAT (also initializes ITD)
    prepareBandpassFilters();

    // Initialize AnalyzerWorker
    int numSlots = 4;
    worker = std::make_unique<AnalyzerWorker>(numSlots, samplesPerBlock, 
                                              numChannels, *this);

}

void AudioAnalyzer::enqueueBlock(const juce::AudioBuffer<float>* buffer)
{
    if (worker && buffer != nullptr)
        worker->pushBlock(*buffer);
}

//=============================================================================
/*  Analyze a block of audio data, compute CQT magnitudes, GCC-PHAT 
    ITDs, and panning index
*/
void AudioAnalyzer::analyzeBlock(const juce::AudioBuffer<float>& buffer)
{
    const std::lock_guard<std::mutex> lock(bufferMutex);

    this->numChannels = buffer.getNumChannels();
    this->numSamples = buffer.getNumSamples();

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

    // Resize CQT magnitudes if channel count changed
    if (static_cast<int>(cqtMagnitudes.size()) != numChannels)
        cqtMagnitudes.resize(static_cast<size_t>(numChannels), std::vector<float>(static_cast<size_t>(numCQTbins), 0.0f));

    // Compute CQT for each channel
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* channelData = analysisBuffer.getReadPointer(ch);
        computeCQT(channelData, ch);  // populate cqtMagnitudes[ch]
    }

    // === ILD/ITD Panning Index Calculation ===
    
    ILDpanningSpectrum.setSize(1, numCQTbins);
    ITDpanningSpectrum.resize(static_cast<size_t>(numCQTbins));
    float maxITD = 0.09f / 343.0f; // ~0.00026 sec --**

    // Only compute panning spectra if stereo (dont know if this if statement is totally necessary)
    if (numChannels == 2)
    {   
        const auto& leftCQT = cqtMagnitudes[0];
        const auto& rightCQT = cqtMagnitudes[1];
        auto* panningData = ILDpanningSpectrum.getWritePointer(0);

        // Compute GCC-PHAT ITD for each CQT band
        computeGCCPHAT_ITD();

        for (size_t k = 0; k < static_cast<size_t>(numCQTbins); ++k)
        {
            // === ITD panning index ===
            float itd = itdPerBand[k]; // in seconds
            ITDpanningSpectrum[k] = juce::jlimit(-1.0f, 1.0f, itd / maxITD); // now in [-1, 1]

            // === ILD panning index ===
            const float L = leftCQT[k];
            const float R = rightCQT[k];

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

    // Combine ILD and ITD into a single panning index
    std::vector<float> combinedPanning(static_cast<size_t>(numCQTbins));
    float ildWeight = 0.5f; // Can change these weights later
    float itdWeight = 0.5f;

    for (size_t b = 0; b < static_cast<size_t>(numCQTbins); ++b)
    {
        float ild = ILDpanningSpectrum.getSample(0, static_cast<int>(b));  // ILD-based index
        float itd = ITDpanningSpectrum[b];               // ITD-based index

        combinedPanning[b] = (ildWeight * ild + itdWeight * itd) / (ildWeight + itdWeight);
    }

    // Store cqtMagnitudes and combinedPanning into "latest" members
    {
        std::lock_guard<std::mutex> resLock(resultsMutex);
        latestCQTMagnitudes = cqtMagnitudes;           // copy vector-of-vectors
        latestCombinedPanning = std::move(combinedPanning); // move
    }

    // bufferMutex and resultsMutex unlocked here
}


// Possibly later on split this into "computeFFT" and "applyCQT" if we need just FFT for other calculations
void AudioAnalyzer::computeCQT(const float* channelData, int channelIndex)
{
    // Load windowed samples (real part) and zero-pad imag part
    for (size_t n = 0; n < static_cast<size_t>(fftSize); ++n)
    {
        float x = (n < static_cast<size_t>(samplesPerBlock)) ? channelData[n] : 0.0f;
        fftData[n] = std::complex<float>(x * window[n], 0.0f);
    }

    // In-place complex FFT (real -> complex)
    // Data layout: [ real[0], real[1], ..., real[fftSize-1], imag[0], imag[1], ..., imag[fftSize-1] ]
    fft->perform(fftData.data(), fftData.data(), false);

    // Compute CQT by inner product with each kernel
    for (size_t bin = 0; bin < static_cast<size_t>(numCQTbins); ++bin)
    {
        std::complex<float> sum = 0.0f;
        const auto& kernel = cqtKernels[bin];
        for (size_t i = 0; i < static_cast<size_t>(fftSize); ++i)
            sum += fftData[i] * std::conj(kernel[i]);

        cqtMagnitudes[static_cast<size_t>(channelIndex)][bin] = std::abs(sum);
    }
}

void AudioAnalyzer::computeGCCPHAT_ITD()
{
    if (numChannels < 2)
        return;

    const float* left = analysisBuffer.getReadPointer(0);
    const float* right = analysisBuffer.getReadPointer(1);

    if (numSamples < fftSize)
        return;

    const float* leftSegment = left + (numSamples - fftSize);
    const float* rightSegment = right + (numSamples - fftSize);

    itdPerBand.resize(static_cast<size_t>(numCQTbins), 0.0f);
    
    for (size_t b = 0; b < static_cast<size_t>(numCQTbins); ++b)
    {
        // Use precomputed bandpass filters
        auto& leftFilter = leftBandpassFilters[b];
        auto& rightFilter = rightBandpassFilters[b];

        // Reset filter states for each new block if needed:
        leftFilter.reset();
        rightFilter.reset();

        float delaySamples = gccPhatDelayPerBand(
            leftSegment,
            rightSegment,
            leftFilter,
            rightFilter
        );

        float normalizedDelay = juce::jlimit(-1.0f, 1.0f, delaySamples / (fftSize / 2.0f));
        itdPerBand[b] = normalizedDelay;
    }
}

// Compute GCC-PHAT delay for a specific frequency band
float AudioAnalyzer::gccPhatDelayPerBand(const float* x, const float* y, 
                                        juce::dsp::IIR::Filter<float>& bandpassLeft, 
                                        juce::dsp::IIR::Filter<float>& bandpassRight)
{
    // Ensure fft is valid
    jassert(fft != nullptr);

    std::vector<float> filteredX(static_cast<size_t>(fftSize));
    std::vector<float> filteredY(static_cast<size_t>(fftSize));

    bandpassLeft.reset();
    bandpassRight.reset();

    // Process samples with given filters
    for (size_t i = 0; i < static_cast<size_t>(fftSize); ++i)
    {
        filteredX[i] = bandpassLeft.processSample(x[i]);
        filteredY[i] = bandpassRight.processSample(y[i]);
    }

    // Windowing
    window.resize(static_cast<size_t>(fftSize));
    for (size_t i = 0; i < static_cast<size_t>(fftSize); ++i)
        window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));

    std::vector<juce::dsp::Complex<float>> X(static_cast<size_t>(fftSize), 0.0f);
    std::vector<juce::dsp::Complex<float>> Y(static_cast<size_t>(fftSize), 0.0f);
    std::vector<juce::dsp::Complex<float>> R(static_cast<size_t>(fftSize));
    std::vector<juce::dsp::Complex<float>> corr(static_cast<size_t>(fftSize));

    for (size_t i = 0; i < static_cast<size_t>(fftSize); ++i)
    {
        X[i] = std::complex<float>(filteredX[i] * window[i], 0.0f);
        Y[i] = std::complex<float>(filteredY[i] * window[i], 0.0f);
    }

    fft->perform(X.data(), X.data(), false);
    fft->perform(Y.data(), Y.data(), false);

    for (size_t i = 0; i < static_cast<size_t>(fftSize); ++i)
    {
        auto Xf = X[i];
        auto Yf = Y[i];
        auto crossSpec = Xf * std::conj(Yf);
        float mag = std::abs(crossSpec);
        R[i] = (mag > 0.0f) ? crossSpec / mag : 0.0f;
    }

    fft->perform(R.data(), corr.data(), true);

    // Find peak in cross-correlation
    int maxIndex = 0;
    float maxValue = -1e10f;
    for (size_t i = 0; i < static_cast<size_t>(fftSize); ++i)
    {
        float val = std::abs(corr[i]);
        if (val > maxValue)
        {
            maxValue = val;
            maxIndex = static_cast<int>(i);
        }
    }

    int center = fftSize / 2;
    int delay = (maxIndex <= center) ? maxIndex : maxIndex - fftSize;

    return static_cast<float>(delay);
}

// Prepare bandpass filters for each CQT center frequency
// (Filter for each CQT bin so that GCC-PHAT can be computed per band)
void AudioAnalyzer::prepareBandpassFilters()
{
    leftBandpassFilters.clear();
    rightBandpassFilters.clear();

    for (float freq : centerFrequencies)
    {
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, freq, 1.0f); // Q = 1

        leftBandpassFilters.emplace_back();
        leftBandpassFilters.back().coefficients = coeffs;

        rightBandpassFilters.emplace_back();
        rightBandpassFilters.back().coefficients = coeffs;
    }

    itdPerBand.resize(static_cast<size_t>(centerFrequencies.size()), 0.0f); // Initialize ITD estimates
}

