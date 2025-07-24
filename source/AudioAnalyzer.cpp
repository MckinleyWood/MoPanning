#include "AudioAnalyzer.h"

//=============================================================================
AudioAnalyzer::AudioAnalyzer() {}
AudioAnalyzer::~AudioAnalyzer()
{
    // Destroy worker, which joins the thread
    stopWorker();
}

//=============================================================================
/*  Prepares the audio analyzer. */
void AudioAnalyzer::prepare()
{
    isPrepared.store(false);

    // Stop any existing worker thread
    stopWorker();

    // Recalculate fftSize - Ensure we are giving it a power of two
    jassert(samplesPerBlock > 0 
         && (samplesPerBlock & (samplesPerBlock - 1)) == 0);
    fftSize = samplesPerBlock;
    numFFTBins = fftSize / 2 + 1;

    // Clear any previously allocated buffers or objects
    window.clear();
    fftBuffer.clear();
    cqtKernels.clear();
    centerFrequencies.clear();

    // Allocate hann window and FFT buffer
    window.resize(fftSize);
    fftBuffer.resize(fftSize);

    // Concise name for pi variable
    float pi = MathConstants<float>::pi;

    // Build the Hann window
    for (int n = 0; n < fftSize; ++n)
        window[n] = 0.5f * (1.0f - std::cos(2.0f * pi * n / (fftSize - 1)));

    // Initialize FFT engine
    fft = std::make_unique<juce::dsp::FFT>((int)std::log2(fftSize));
    
    // Calculate maximum expected FFT bin magnitude
    // Currently we are using a hardcoded value
    // maxExpectedMag = 0.0;
    // for (auto w : window)
    //     maxExpectedMag += w;
    // maxExpectedMag /= 2; // Reasonable signals will be in this range

    // Set frequency from min to Nyquist
    const float nyquist = static_cast<float>(sampleRate * 0.5);
    const float logMin = std::log2(minCQTfreq);
    const float logMax = std::log2(nyquist);

    // Prepare CQT kernels
    cqtKernels.resize(numCQTbins);
    centerFrequencies.resize(numCQTbins);

    // Precompute CQT kernels
    for (int bin = 0; bin < numCQTbins; ++bin)
    {
        // Compute center frequency for this bin
        float frac = bin * 1.0f / (numCQTbins + 1);
        float freq = std::pow(2.0f, logMin + frac * (logMax - logMin));
        centerFrequencies[bin] = freq;

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

        // Allocate JUCE-compatible buffers
        juce::HeapBlock<juce::dsp::Complex<float>> fftInput(kernelLength);
        juce::HeapBlock<juce::dsp::Complex<float>> fftOutput(kernelLength);

        // Copy kernelTime to JUCE-compatible input
        for (int i = 0; i < kernelLength; ++i)
            fftInput[i] = kernelTime[i];

        // Zero-pad and FFT-- convert to frequency domain
        juce::dsp::FFT kernelFFT((int)std::log2(kernelLength));
        kernelFFT.perform(fftInput, fftOutput, false);

        // Copy result back into std::vector<std::complex<float>>
        std::vector<std::complex<float>> kernelFreq(kernelLength);
        for (int i = 0; i < kernelLength; ++i)
            kernelFreq[i] = fftOutput[i];

        cqtKernels[bin] = std::move(kernelFreq);

        float energy = 0.0f;
        for (const auto& c : kernelFreq)
            energy += std::abs(c);
    }

    // Estimate memory use for CQT kernels
    auto bytes = numCQTbins * fftSize * sizeof(std::complex<float>);
    // DBG("Estimated CQT kernel memory use: " << (bytes / 1024.0f) << " KB"); 

    // Prepare bandpass filters for GCC-PHAT (also initializes ITD)
    prepareBandpassFilters();

    // Initialize AnalyzerWorker
    int numSlots = 4;
    worker = std::make_unique<AnalyzerWorker>(
        numSlots, samplesPerBlock, *this);
    worker->start();

    isPrepared.store(true);
}

void AudioAnalyzer::enqueueBlock(const juce::AudioBuffer<float>* buffer)
{
    if (worker && buffer != nullptr)
        worker->pushBlock(*buffer);
}

void AudioAnalyzer::stopWorker()
{
    if (worker)
    {
        worker->stop();    // Stop cleanly
        worker.reset();    // Then delete
    }
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

void AudioAnalyzer::setSampleRate(double newSampleRate)
{
    this->sampleRate = newSampleRate;
}

void AudioAnalyzer::setTransform(Transform newTransform)
{
    transform = newTransform;
    DBG("new analysis mode = " << static_cast<int>(newTransform));
}

void AudioAnalyzer::setPanMethod(PanMethod newPanMethod)
{
    panMethod = newPanMethod;
    DBG("new pan method = " << static_cast<int>(newPanMethod));
}


void AudioAnalyzer::setFFTOrder(float newFFTOrder)
{
    fftOrder = newFFTOrder;
}

void AudioAnalyzer::setMinFrequency(float newMinFrequency)
{
    minCQTfreq = newMinFrequency;
}

void AudioAnalyzer::setNumCQTBins(float newNumCQTBins)
{
    numCQTbins = newNumCQTBins;
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
}

/*  Computes the FFT of each channel of the input buffer and stores the
    results in outSpectra.
*/
void AudioAnalyzer::computeFFT(const juce::AudioBuffer<float>& buffer,
                               std::array<fft_buffer_t, 2>& outSpectra)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        // Copy & window the buffer data
        auto* readPtr = buffer.getReadPointer(ch);
        for (int n = 0; n < fftSize; ++n)
        {
            float s = readPtr[n];
            fftBuffer[n].real(s * window[n]);
            fftBuffer[n].imag(0.0f);
        }

        fft->perform(fftBuffer.data(), fftBuffer.data(), false);

        outSpectra[ch].resize(fftSize);
        jassert(fftBuffer.size() == fftSize);
        jassert(outSpectra[ch].size() == fftSize);

        for (int b = 0; b < fftSize; ++b)
            outSpectra[ch][b] = fftBuffer[b];
    }
}

/*  Computes the inter-channel level difference for each frequency bin
    and stores the results in panIndices.
*/
void AudioAnalyzer::computeILDs(std::array<std::vector<float>, 2>& magnitudes,
                                int numBands, std::vector<float>& panIndices)
{
    panIndices.resize(numBands);

    for (int b = 0; b < numBands; ++b)
    {
        float L = magnitudes[0][b];
        float R = magnitudes[1][b];
        float denom = L * L + R * R + 1e-12f; // Avoid zero
        float sim = (2.0f * L * R) / denom;

        // Direction: L > R => left => -1; R > L => right => +1
        float dir = (L > R ? -1.0f : (R > L ? 1.0f : 0.0f));

        // Final pan is in the range [-1, +1]
        panIndices[b] = dir * (1.0f - sim);
    }
}

/*  Computes the CQT of an audio buffer given the FFT results and stores
    the magnitudes (one for each channel and CQT bin) in cqtMags.
*/
void AudioAnalyzer::computeCQT(const juce::AudioBuffer<float>& buffer,
                               std::array<fft_buffer_t, 2> ffts,
                               std::array<std::vector<float>, 2>& cqtMags)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        cqtMags[ch].resize(numCQTbins, 0.0f);
        
        jassert(ffts[ch].size() == fftSize);

        // Compute CQT by inner product with each kernel
        for (int bin = 0; bin < numCQTbins; ++bin)
        {
            jassert(bin < cqtKernels.size());
            jassert(cqtKernels[bin].size() == fftSize);

            std::complex<float> sum = 0.0f;
            const auto& kernel = cqtKernels[bin];
            for (int i = 0; i < fftSize; ++i)
                sum += ffts[ch][i] * std::conj(kernel[i]);

            cqtMags[ch][bin] = std::abs(sum);
        }
    }

    // juce::StringArray mags;
    // for (float m : cqtMags[0])
    //     mags.add(juce::String(m, 3));
    // DBG("CQT magnitudes (post inner product) for channel 0: " 
    //     << mags.joinIntoString(", ")); 
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

    // DBG("filteredX[0] = " << filteredX[0] 
    //  << ", filteredY[0] = " << filteredY[0]);

    std::vector<juce::dsp::Complex<float>> X(fftSize, 0.0f);
    std::vector<juce::dsp::Complex<float>> Y(fftSize, 0.0f);
    std::vector<juce::dsp::Complex<float>> R(fftSize);
    std::vector<juce::dsp::Complex<float>> corr(fftSize);

    for (int i = 0; i < fftSize; ++i)
    {
        X[i] = std::complex<float>(filteredX[i] * window[i], 0.0f);
        Y[i] = std::complex<float>(filteredY[i] * window[i], 0.0f);
    }

    fft->perform(X.data(), X.data(), false);
    fft->perform(Y.data(), Y.data(), false);

    for (int i = 0; i < fftSize; ++i)
    {
        auto Xf = X[i];
        auto Yf = Y[i];
        auto crossSpec = Xf * std::conj(Yf);
        float mag = std::abs(crossSpec);
        R[i] = (mag > 0.0f) ? crossSpec / mag : 0.0f;
    }

    // DBG("R[0] = " << juce::String(R[0].real()) 
    //  << " + " << juce::String(R[0].imag()) + "i, "
    //  << "corr[0] = " + juce::String(corr[0].real()) 
    //  << " + " + juce::String(corr[0].imag()) + "i");

    fft->perform(R.data(), corr.data(), true);

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

    // DBG("GCC-PHAT delay: maxIndex = " << maxIndex
    //     << ", delay = " << delay
    //     << ", maxCorr = " << maxValue);

    return static_cast<float>(delay);

    // float energyX = std::accumulate(filteredX.begin(), filteredX.end(), 0.0f,
    // [](float acc, float v) { return acc + v*v; });
    // float energyY = std::accumulate(filteredY.begin(), filteredY.end(), 0.0f,
    // [](float acc, float v) { return acc + v*v; });

    // DBG("Filtered signal energy: X = " << energyX << ", Y = " << energyY);
}

void AudioAnalyzer::computeGCCPHAT_ITD(const juce::AudioBuffer<float>& buffer,
                                       int numBands, 
                                       std::vector<float>& panIndices)
                                       
{
    const float* left = buffer.getReadPointer(0);
    const float* right = buffer.getReadPointer(1);

    if (samplesPerBlock < fftSize)
        return;

    const float* leftSegment = left + (samplesPerBlock - fftSize);
    const float* rightSegment = right + (samplesPerBlock - fftSize);

    panIndices.resize(numBands, 0.0f);
    
    for (int b = 0; b < numBands; ++b)
    {
        // Use precomputed bandpass filters
        auto& leftFilter = leftBandpassFilters[b];
        auto& rightFilter = rightBandpassFilters[b];

        // Reset filter states for each new block if needed:
        leftFilter.reset();
        rightFilter.reset();

        float delaySamples = gccPhatDelayPerBand(leftSegment, rightSegment,
                                                 leftFilter, rightFilter);

        float delaySeconds = delaySamples / sampleRate;
        panIndices[b] = juce::jlimit(-1.0f, 1.0f, delaySeconds / maxITD);

        DBG("ITD bin " << b
            << ": delaySamples = " << delaySamples
            << ", delaySeconds = " << delaySeconds);
    }
}

//=============================================================================
void AudioAnalyzer::analyzeBlock(const juce::AudioBuffer<float>& buffer)
{
    std::array<std::vector<float>, 2> magnitudes;
    std::vector<float> ilds;
    std::vector<float> itds;
    std::vector<float> panIndices;
    int numBands = 0;
    float ildWeight = 0.75f; // Can change these weights later
    float itdWeight = 0.25f;
    
    // Compute FFT for the block
    std::array<fft_buffer_t, 2> ffts;
    computeFFT(buffer, ffts);

    // Compute the selected frequency transform for the signal
    switch (transform)
    {
    case FFT:
        // Copy magnitudes to spectra
        for (int ch = 0; ch < 2; ++ch)
        {
            magnitudes[ch].resize(numFFTBins);
            for (int b = 0; b < numFFTBins; ++b)
                magnitudes[ch][b] = std::abs(ffts[ch][b]);
        }
        numBands = numFFTBins;
        break;
    
    case CQT:
        // Compute CQT magnitudes
        computeCQT(buffer, ffts, magnitudes);
        numBands = numCQTbins;
        break;

    default:
        jassertfalse;
        return;
    }

    // Compute panning indices based on the selected pan method
    switch (panMethod)
    {
    case level_pan:
        // Use ILD pan indices
        computeILDs(magnitudes, numBands, panIndices);
        break;
    
    case time_pan:
        // Use ITD pan indices
        panIndices.resize(numBands);
        break;
    
    case both:
        computeILDs(magnitudes, numBands, ilds);
        computeGCCPHAT_ITD(buffer, numBands, itds);

        panIndices.resize(numBands);

        for (int b = 0; b < numCQTbins; ++b)
        {
            panIndices[b] = (ildWeight * ilds[b] + itdWeight * itds[b]) 
                          / (ildWeight + itdWeight);
        }
        break;

    default:
        jassertfalse;
        return;
    }

    // Build frequency_band list
    std::vector<frequency_band> newResults;
    newResults.reserve(numBands);
    float binWidth;

    switch (transform)
    {
    case FFT:
        binWidth = (float)sampleRate / numBands;
        for (int b = 0; b < numBands; ++b)
        {
            float freq = b * binWidth;
            float ampL = std::abs(magnitudes[0][b]);
            float ampR = std::abs(magnitudes[1][b]);
            float amp  = (ampL + ampR) * 0.5f; // Average amplitude
            amp = juce::jlimit(0.0f, 1.0f, amp / 150);
            newResults.push_back({ freq, amp, panIndices[b] });
        }
        break;

    case CQT:
        for (int b = 0; b < numBands; ++b)
        {
            float freq = centerFrequencies[b];
            float ampL = std::abs(magnitudes[0][b]);
            float ampR = std::abs(magnitudes[1][b]);
            float amp  = (ampL + ampR) * 0.5f; // Average amplitude
            amp = juce::jlimit(0.0f, 1.0f, amp / 6000);
            newResults.push_back({ freq, amp, panIndices[b] });
        }
        break;
    
    default:
        break;
    }

    // Hand results to GUI
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        results = std::move(newResults);
    }
}