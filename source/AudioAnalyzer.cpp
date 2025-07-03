#include "AudioAnalyzer.h"

//=============================================================================
AudioAnalyzer::AudioAnalyzer() {}
AudioAnalyzer::~AudioAnalyzer()
{
    // Destroy worker, which joins the thread
    worker.reset();
}

//=============================================================================
/*  */
void AudioAnalyzer::prepare(int samplesPerBlock, double sampleRate)
{
    DBG("Preparing AudioAnalyzer...");
    this->samplesPerBlock = samplesPerBlock;
    this->sampleRate = sampleRate;

    DBG("Block size = " << this->samplesPerBlock);
    DBG("Sample rate = " << this->sampleRate);

    // Allocate hann window and FFT buffer
    window.resize(fftSize);
    fftBuffer.resize(fftSize);

    // Build the Hann window
    float pi = MathConstants<float>::pi;
    for (int n = 0; n < fftSize; ++n)
        window[n] = 0.5f * (1.0f - std::cos(2.0f * pi * n / (fftSize - 1)));

    // Calculate maximum expected FFT bin magnitude
    double windowSum = 0.0;
    for (auto w : window)
        windowSum += w;
    maxExpectedMag = static_cast<float>(windowSum);

    // Prepare CQT kernels
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
        float frac = static_cast<float>(bin) / static_cast<float>(numCQTbins + 1);
        float freq = std::pow(2.0, logMin + frac * (logMax - logMin)); // Log-spaced
        centerFrequencies[bin] = freq;
        DBG("Center frequency for bin " << bin << " = " << freq);

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
    worker = std::make_unique<AnalyzerWorker>(
        numSlots, samplesPerBlock, *this);
}

void AudioAnalyzer::enqueueBlock(const juce::AudioBuffer<float>* buffer)
{
    if (worker && buffer != nullptr)
        worker->pushBlock(*buffer);
}

std::vector<frequency_band> AudioAnalyzer::getLatestResults() const
{
    std::lock_guard<std::mutex> lock(resultsMutex);
    return results;
}

void AudioAnalyzer::setSamplesPerBlock(int newSamplesPerBlock)
{
    this->samplesPerBlock = newSamplesPerBlock;
}

void AudioAnalyzer::setSampleRate(int newSampleRate)
{
    this->sampleRate = newSampleRate;
}

//=============================================================================
/*  Computes the FFT of each channel of the input buffer and stores the
    results in outSpectra.
*/
void AudioAnalyzer::computeFFT(const juce::AudioBuffer<float>& buffer,
                               std::array<fft_buffer_t, 2>& outSpectra)
{
    int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < 2; ++ch)
    {
        // Copy & window the buffer data
        auto* readPtr = buffer.getReadPointer(ch);
        for (int n = 0; n < fftSize; ++n)
        {
            float s = (n < numSamples ? readPtr[n] : 0.0f);
            fftBuffer[n].real(s * window[n]);
            fftBuffer[n].imag(0.0f);
        }

        fft.perform(fftBuffer.data(), fftBuffer.data(), false);

        // Extract just the first half (up to the Nyquist frequency)
        outSpectra[ch].resize(numBins);
        for (int b = 0; b < numBins; ++b)
            outSpectra[ch][b] = fftBuffer[b];
    }
}

/*  Computes the inter-channel level difference for each frequency bin
    and stores the results in outPan. As this is basically straight from
    ChatGPT, it would be worth checking whether it is really the same as
    in the MarPanning paper.
*/
void AudioAnalyzer::computeILDs(std::array<fft_buffer_t, 2>& spectra,
                                std::vector<float>& outPan)
{
    outPan.resize(numBins);

    for (int b = 0; b < numBins; ++b)
    {
        float L = std::abs(spectra[0][b]);
        float R = std::abs(spectra[1][b]);
        float denom = L*L + R*R + 1e-12f; // Avoid zero
        float sim = (2.0f * L * R) / denom;

        // Direction: L > R => left => -1; R > L => right => +1
        float dir = (L > R ? -1.0f : (R > L ? 1.0f : 0.0f));

        // Final pan is in the range [-1, +1]
        outPan[b] = dir * (1.0f - sim);
    }
}

/* Analyze a block of audio data - MCKINLEY FFT + ILD VERSION */
void AudioAnalyzer::analyzeBlockFFT(const juce::AudioBuffer<float>& buffer)
{
    // DBG("Analyzing the next audio block");

    // FFT & get complex spectra
    std::array<fft_buffer_t, 2> spectra;
    computeFFT(buffer, spectra);

    // Compute ILD pan indices
    std::vector<float> panIndex;
    computeILDs(spectra, panIndex);

    // Build frequency_band list
    std::vector<frequency_band> newResults;
    newResults.reserve(numBins);

    float binWidth = (float)sampleRate / (float)fftSize;
    for (int b = 0; b < numBins; ++b)
    {
        float freq = b * binWidth;
        float ampL = std::abs(spectra[0][b]);
        float ampR = std::abs(spectra[1][b]);
        float amp  = (ampL + ampR) * 0.5f; // Average amplitude
        amp = juce::jlimit(0.0f, 1.0f, amp / maxExpectedMag);
        amp = sqrt(amp);
        newResults.push_back({ freq, amp, panIndex[b] });
    }

    // Hand results to GUI
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        results = std::move(newResults);
    }
}

/* Analyze a block of audio data - OWEN VERSION */
void AudioAnalyzer::analyzeBlockCQT(const juce::AudioBuffer<float>& buffer)
{
    // Compute CQT for each channel
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* channelData = buffer.getReadPointer(ch);
        computeCQT(channelData, ch);  // populate cqtMagnitudes[ch]
    }

    // === ILD/ITD Panning Index Calculation ===
    
    ILDpanningSpectrum.setSize(1, numCQTbins);
    ITDpanningSpectrum.resize(static_cast<size_t>(numCQTbins));
    float maxITD = 0.09f / 343.0f; // ~0.00026 sec --**

    const auto& leftCQT = cqtMagnitudes[0];
    const auto& rightCQT = cqtMagnitudes[1];
    auto* panningData = ILDpanningSpectrum.getWritePointer(0);

    // Compute GCC-PHAT ITD for each CQT band
    computeGCCPHAT_ITD(buffer);

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

    // Build list of frequency_band structs (as in Mckinley version)

    // Hand over to GUI
}


// Possibly later on split this into "computeFFT" and "applyCQT" if we need just FFT for other calculations
void AudioAnalyzer::computeCQT(const float* channelData, int channelIndex)
{
    // Load windowed samples (real part) and zero-pad imag part
    for (size_t n = 0; n < static_cast<size_t>(fftSize); ++n)
    {
        float x = (n < static_cast<size_t>(samplesPerBlock)) ? channelData[n] : 0.0f;
        fftBuffer[n] = std::complex<float>(x * window[n], 0.0f);
    }

    // In-place complex FFT (real -> complex)
    // Data layout: [ real[0], real[1], ..., real[fftSize-1], imag[0], imag[1], ..., imag[fftSize-1] ]
    fft.perform(fftBuffer.data(), fftBuffer.data(), false);

    // Compute CQT by inner product with each kernel
    for (size_t bin = 0; bin < static_cast<size_t>(numCQTbins); ++bin)
    {
        std::complex<float> sum = 0.0f;
        const auto& kernel = cqtKernels[bin];
        for (size_t i = 0; i < static_cast<size_t>(fftSize); ++i)
            sum += fftBuffer[i] * std::conj(kernel[i]);

        cqtMagnitudes[static_cast<size_t>(channelIndex)][bin] = std::abs(sum);
    }
}

void AudioAnalyzer::computeGCCPHAT_ITD(const juce::AudioBuffer<float>& buffer)
{
    const float* left = buffer.getReadPointer(0);
    const float* right = buffer.getReadPointer(1);

    if (samplesPerBlock < fftSize)
        return;

    const float* leftSegment = left + (samplesPerBlock - fftSize);
    const float* rightSegment = right + (samplesPerBlock - fftSize);

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

    fft.perform(X.data(), X.data(), false);
    fft.perform(Y.data(), Y.data(), false);

    for (size_t i = 0; i < static_cast<size_t>(fftSize); ++i)
    {
        auto Xf = X[i];
        auto Yf = Y[i];
        auto crossSpec = Xf * std::conj(Yf);
        float mag = std::abs(crossSpec);
        R[i] = (mag > 0.0f) ? crossSpec / mag : 0.0f;
    }

    fft.perform(R.data(), corr.data(), true);

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

