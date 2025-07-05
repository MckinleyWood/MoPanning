#include "AudioAnalyzer.h"

//=============================================================================
AudioAnalyzer::AudioAnalyzer() {}
AudioAnalyzer::~AudioAnalyzer()
{
    // Destroy worker, which joins the thread
    worker.reset();
}

//=============================================================================
/*  Prepares the audio analyzer. */
void AudioAnalyzer::prepare(int samplesPerBlock, double sampleRate)
{
    DBG("Preparing AudioAnalyzer...");
    this->samplesPerBlock = samplesPerBlock;
    this->sampleRate = sampleRate;

    DBG("Block size = " << this->samplesPerBlock);
    DBG("Sample rate = " << this->sampleRate);

    // Concise name for pi variable
    float pi = MathConstants<float>::pi;

    // Allocate hann window and FFT buffer
    window.resize(fftSize);
    fftBuffer.resize(fftSize);

    // Build the Hann window
    for (int n = 0; n < fftSize; ++n)
        window[n] = 0.5f * (1.0f - std::cos(2.0f * pi * n / (fftSize - 1)));

    // Calculate maximum expected FFT bin magnitude
    maxExpectedMag = 0.0;
    for (auto w : window)
        maxExpectedMag += w;

    // Prepare CQT kernels
    cqtKernels.clear();
    cqtKernels.resize(numCQTbins);
    centerFrequencies.resize(numCQTbins);

    // Set frequency from min to Nyquist
    const float nyquist = static_cast<float>(sampleRate * 0.5);
    const float logMin = std::log2(minCQTfreq);
    const float logMax = std::log2(nyquist);

    // Precompute CQT kernels
    for (int bin = 0; bin < numCQTbins; ++bin)
    {
        // Compute center frequency for this bin
        float frac = bin * 1.0f / (numCQTbins + 1);
        float freq = std::pow(2.0f, logMin + frac * (logMax - logMin));
        centerFrequencies[bin] = freq;
        DBG("Center frequency for bin " << bin << " = " << freq);

        // Generate complex sinusoid for this frequency (for inner products)
        int kernelLength = fftSize;
        std::vector<std::complex<float>> kernelTime(kernelLength, 0.0f);

        for (int n = 0; n < kernelLength; ++n)
        {
            // Edited for centering
            float t = (n - fftSize * 0.5f) / float(sampleRate); 
            float w = 0.5f * (1.0f - std::cos(2.0f * pi * n 
                                            / (kernelLength - 1)));
            // Complex sinusioid * window
            kernelTime[n] = std::polar(w, -2.0f * pi * freq * t); 
        }

        // Normalize the kernel energy
        float norm = 0.0f;
        for (auto& v : kernelTime)
            norm += std::norm(v);
        norm = std::sqrt(norm);
        for (auto& v : kernelTime)
            v /= norm;

        // Zero-pad and FFT-- convert to frequency domain
        std::vector<std::complex<float>> kernelFreq(kernelLength);
        juce::dsp::FFT kernelFFT((std::log2(kernelLength)));
        kernelFFT.perform(kernelTime.data(), kernelFreq.data(), false);

        cqtKernels[bin] = std::move(kernelFreq);

        float energy = 0.0f;
        for (const auto& c : kernelFreq)
            energy += std::abs(c);
        DBG("Kernel " << bin << " energy = " << energy);

        DBG("Built " << cqtKernels.size() << " CQT kernels");
    }

    // Allocate CQT result: 2 channels default
    cqtMagnitudes.resize(2, std::vector<float>(numCQTbins, 0.0f));

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
/*  Prepare bandpass filters for each CQT center frequency. Filter for 
    each CQT bin so that GCC-PHAT can be computed per band.
*/
void AudioAnalyzer::prepareBandpassFilters()
{
    leftBandpassFilters.clear();
    rightBandpassFilters.clear();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(fftSize);
    spec.numChannels = 1; // Mono processing for each channel

    for (float freq : centerFrequencies)
    {
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(
            sampleRate, freq, Q);

        juce::dsp::IIR::Filter<float> leftFilter;
        juce::dsp::IIR::Filter<float> rightFilter;

        leftFilter.prepare(spec);
        rightFilter.prepare(spec);

        leftFilter.coefficients = coeffs;
        rightFilter.coefficients = coeffs;

        leftBandpassFilters.push_back(std::move(leftFilter));
        rightBandpassFilters.push_back(std::move(rightFilter));
    }

    // Initialize ITD estimates
    itdPerBand.resize(centerFrequencies.size(), 0.0f); 
}

/*  Computes the FFT of each channel of the input buffer and stores the
    results in outSpectra.
*/
void AudioAnalyzer::computeFFT(const juce::AudioBuffer<float>& buffer,
                               std::array<fft_buffer_t, 2>& outSpectra)
{
    int numSamples = (buffer.getNumSamples());
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

        DBG("First few FFT buffer values after transform:");
        for (int i = 0; i < 5; ++i)
        {
            DBG("fftBuffer[" << i << "] = " << fftBuffer[i].real() 
             << " + " << fftBuffer[i].imag() << "i");
        }

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

/*  Computes the CQT of a channel. We could possibly split this into 
    "computeFFT" and "applyCQT" if we need just FFT for other 
    calculations.
*/
void AudioAnalyzer::computeCQT(const float* channelData, int channelIndex)
{
    // Use end of buffer to match GCC-PHAT (if large enough)
    const float* input = channelData;
    if (samplesPerBlock >= fftSize)
        input = channelData + (samplesPerBlock - fftSize);

    DBG("Reading input from offset: " << (input - channelData));

    // Load windowed samples (real part) and zero-pad imag part
    for (int n = 0; n < fftSize; ++n)
    {
        float x = (n < (samplesPerBlock)) ? input[n] : 0.0f;
        fftBuffer[n] = std::complex<float>(x * window[n], 0.0f);
    }

    // In-place complex FFT (real -> complex)
    // Data layout: [ real[0], real[1], ..., real[fftSize-1], 
    // imag[0], imag[1], ..., imag[fftSize-1] ]
    fft.perform(fftBuffer.data(), fftBuffer.data(), false);

    // Compute CQT by inner product with each kernel
    for (int bin = 0; bin < numCQTbins; ++bin)
    {
        std::complex<float> sum = 0.0f;
        const auto& kernel = cqtKernels[bin];
        for (int i = 0; i < fftSize; ++i)
            sum += fftBuffer[i] * std::conj(kernel[i]);

        cqtMagnitudes[(channelIndex)][bin] = std::abs(sum);
    }

    // juce::StringArray mags;
    // for (float m : cqtMagnitudes[channelIndex])
    //     mags.add(juce::String(m, 3));
    // DBG("CQT magnitudes (post inner product) for ch " << channelIndex 
    //  << ": " << mags.joinIntoString(", "));
}

/*  Compute GCC-PHAT delay for a specific frequency band */
float AudioAnalyzer::gccPhatDelayPerBand(const float* x, const float* y, 
                                juce::dsp::IIR::Filter<float>& bandpassLeft, 
                                juce::dsp::IIR::Filter<float>& bandpassRight)
{
    std::vector<float> filteredX(fftSize);
    std::vector<float> filteredY(fftSize);

    bandpassLeft.reset();
    bandpassRight.reset();

    // Process samples with given filters
    for (int i = 0; i < fftSize; ++i)
    {
        filteredX[i] = bandpassLeft.processSample(x[i]);
        filteredY[i] = bandpassRight.processSample(y[i]);
    }

    DBG("filteredX[0] = " << filteredX[0] 
     << ", filteredY[0] = " << filteredY[0]);

    std::vector<juce::dsp::Complex<float>> X(fftSize, 0.0f);
    std::vector<juce::dsp::Complex<float>> Y(fftSize, 0.0f);
    std::vector<juce::dsp::Complex<float>> R(fftSize);
    std::vector<juce::dsp::Complex<float>> corr(fftSize);

    for (int i = 0; i < fftSize; ++i)
    {
        X[i] = std::complex<float>(filteredX[i] * window[i], 0.0f);
        Y[i] = std::complex<float>(filteredY[i] * window[i], 0.0f);
    }

    fft.perform(X.data(), X.data(), false);
    fft.perform(Y.data(), Y.data(), false);

    for (int i = 0; i < fftSize; ++i)
    {
        auto Xf = X[i];
        auto Yf = Y[i];
        auto crossSpec = Xf * std::conj(Yf);
        float mag = std::abs(crossSpec);
        R[i] = (mag > 0.0f) ? crossSpec / mag : 0.0f;
    }

    DBG("R[0] = " << juce::String(R[0].real()) 
     << " + " << juce::String(R[0].imag()) + "i, "
     << "corr[0] = " + juce::String(corr[0].real()) 
     << " + " + juce::String(corr[0].imag()) + "i");

    fft.perform(R.data(), corr.data(), true);

    // Find peak in cross-correlation
    int maxIndex = 0;
    float maxValue = -1e10f;
    for (int i = 0; i < fftSize; ++i)
    {
        float val = std::abs(corr[i]);
        if (val > maxValue)
        {
            maxValue = val;
            maxIndex = (i);
        }
    }

    int center = fftSize / 2;
    int delay = (maxIndex <= center) ? maxIndex : maxIndex - fftSize;

    DBG("GCC-PHAT delay: maxIndex = " << maxIndex
        << ", delay = " << delay
        << ", maxCorr = " << maxValue);

    return static_cast<float>(delay);

    float energyX = std::accumulate(filteredX.begin(), filteredX.end(), 0.0f,
    [](float acc, float v) { return acc + v*v; });
    float energyY = std::accumulate(filteredY.begin(), filteredY.end(), 0.0f,
    [](float acc, float v) { return acc + v*v; });

    DBG("Filtered signal energy: X = " << energyX << ", Y = " << energyY);
}

void AudioAnalyzer::computeGCCPHAT_ITD(const juce::AudioBuffer<float>& buffer)
{
    const float* left = buffer.getReadPointer(0);
    const float* right = buffer.getReadPointer(1);

    if (samplesPerBlock < fftSize)
        return;

    const float* leftSegment = left + (samplesPerBlock - fftSize);
    const float* rightSegment = right + (samplesPerBlock - fftSize);

    itdPerBand.resize(numCQTbins, 0.0f);
    
    for (int b = 0; b < numCQTbins; ++b)
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

        itdPerBand[b] = delaySamples / sampleRate;

        DBG("ITD bin " << b 
            << ": delaySamples = " << delaySamples
            << ", delaySeconds = " << itdPerBand[b]);
    }
}

//=============================================================================
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
    DBG("Analyzing the next audio block");

    DBG("numCQTbins = " << numCQTbins << ", fftSize = " << fftSize);

    // Compute CQT for each channel
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* channelData = buffer.getReadPointer(ch);
        computeCQT(channelData, ch);  // populate cqtMagnitudes[ch]
    }

    // for (int ch = 0; ch < 2; ++ch)
    // {
    //     juce::StringArray mags;
    //     for (float m : cqtMagnitudes[ch])
    //         mags.add(juce::String(m, 3));
    //     DBG("CQT magnitudes (ch " << ch << "): " 
    //      << mags.joinIntoString(", "));
    // }

    // === ILD/ITD Panning Index Calculation ===
    
    ILDpanningSpectrum.setSize(1, numCQTbins);
    ITDpanningSpectrum.resize(numCQTbins);

    const auto& leftCQT = cqtMagnitudes[0];
    const auto& rightCQT = cqtMagnitudes[1];
    auto* panningData = ILDpanningSpectrum.getWritePointer(0);

    // Compute GCC-PHAT ITD for each CQT band
    computeGCCPHAT_ITD(buffer);

    for (int k = 0; k < numCQTbins; ++k)
    {
        // === ITD panning index ===
        ITDpanningSpectrum[k] = juce::jlimit(-1.0f, 1.0f, itdPerBand[k] / maxITD);

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

    // Log ITD and ILD panning values
    juce::StringArray itdStrs, ildStrs;
    for (int k = 0; k < numCQTbins; ++k)
    {
        itdStrs.add(juce::String(ITDpanningSpectrum[k], 3));
        ildStrs.add(juce::String(ILDpanningSpectrum.getSample(0, k), 3));
    }
    DBG("ITD panning: " << itdStrs.joinIntoString(", "));
    DBG("ILD panning: " << ildStrs.joinIntoString(", "));


    // Combine ILD and ITD into a single panning index
    std::vector<float> combinedPanning(numCQTbins);
    float ildWeight = 0.75f; // Can change these weights later
    float itdWeight = 0.25f;

    for (int b = 0; b < numCQTbins; ++b)
    {
        float ild = ILDpanningSpectrum.getSample(0, (b)); 
        float itd = ITDpanningSpectrum[b];

        combinedPanning[b] = (ildWeight * ild + itdWeight * itd) 
                           / (ildWeight + itdWeight);
    }

    juce::StringArray combinedStrs;
    for (int k = 0; k < numCQTbins; ++k)
        combinedStrs.add(juce::String(combinedPanning[k], 3));
    DBG("Combined panning indices: " 
     << combinedStrs.joinIntoString(", "));


    // Construct frequency_band results
    std::vector<frequency_band> newResults;
    newResults.reserve(numCQTbins);
    for (int k = 0; k < numCQTbins; ++k)
    {
        float freq = centerFrequencies[k];
        float amp = (leftCQT[k] + rightCQT[k]) * 0.5f;
        amp = juce::jlimit(0.0f, 1.0f, amp / maxExpectedMag);  // normalize
        amp = std::sqrt(amp);  // same perceptual scaling as in FFT version
        newResults.push_back({ freq, amp, combinedPanning[k] });
    }

    juce::StringArray bandStrs;
    for (int k = 0; k < std::min((10), numCQTbins); ++k)
    {
        auto& band = newResults[k];
        bandStrs.add("f=" + juce::String(band.frequency, 1) +
                    " A=" + juce::String(band.amplitude, 3) +
                    " P=" + juce::String(band.pan_index, 3));
    }
    DBG("First 10 frequency_band structs: " << bandStrs.joinIntoString(" | "));

    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        results = std::move(newResults);
    }
}

