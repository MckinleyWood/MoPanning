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
void AudioAnalyzer::prepareToPlay(int newSamplesPerBlock, double newSampleRate)
{
    if (isPrepared.load())
        return; // Already prepared

    // Stop any existing worker thread
    stopWorker();

    samplesPerBlock = newSamplesPerBlock;
    sampleRate = newSampleRate;

    // Recalculate fftSize - Ensure we are giving it a power of two
    jassert(samplesPerBlock > 0);
    jassert((samplesPerBlock & (samplesPerBlock - 1)) == 0);
    fftSize = samplesPerBlock;
    numFFTBins = fftSize / 2 + 1;

    fftScaleFactor = 4.0f / fftSize;
    cqtScaleFactor = fftScaleFactor / 28.0f; // Empirical normalization factor

    // Clear any previously allocated buffers or objects
    window.clear();
    fftBuffer.clear();
    cqtKernels.clear();
    centerFrequencies.clear();
    frequencyWeights.clear();

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

    if (transform == FFT)
    {
        float binWidth = (sampleRate / 2.f) / numFFTBins;
        centerFrequencies.resize(numFFTBins);
        for (int b = 0; b < numFFTBins; ++b)
        {
            centerFrequencies[b] = b * binWidth;
        }
    }
    else if (transform == CQT)
        setupCQT();
    else
        jassertfalse; // Unknown transform type

    // Set up frequency-weighting factors
    if (freqWeighting == A_weighting)
        setupAWeights(centerFrequencies, frequencyWeights);
    else
        frequencyWeights.resize(centerFrequencies.size(), 1.0f);

    // Compute frequency-dependent ITD/ILD weights
    itdWeights.resize(numCQTbins);
    ildWeights.resize(numCQTbins);
    maxITD.resize(numCQTbins);
    for (int bin = 0; bin < numCQTbins; ++bin)
    {
        // Best-fit curve
        float ITDweight = 1.0f / (1.0f + std::pow(centerFrequencies[bin] 
                                                / f_trans, p));
        float ILDweight = 1.0f - ITDweight;

        itdWeights[bin] = ITDweight;
        ildWeights[bin] = ILDweight;

        // Smooth exponential decay from ITD_low to ITD_high
        maxITD[bin] = maxITDhigh + (maxITDlow - maxITDhigh) 
                                 * std::exp(-centerFrequencies[bin] / 2000.0f);
    } 

    // Initialize AnalyzerWorker
    int numSlots = 4;
    worker = std::make_unique<AnalyzerWorker>(
        numSlots, samplesPerBlock, *this);
    worker->start();

    isPrepared.store(true);
}

void AudioAnalyzer::prepareToPlay()
{
    // Use current sampleRate and samplesPerBlock if none specified
    prepareToPlay(samplesPerBlock, sampleRate);
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

void AudioAnalyzer::setTransform(Transform newTransform)
{
    if (newTransform == transform) return; // No change
    if (worker) stopWorker(); // Stop worker while we change the parameter

    transform = newTransform;
    isPrepared.store(false);
}

void AudioAnalyzer::setPanMethod(PanMethod newPanMethod)
{
    panMethod = newPanMethod;
}

void AudioAnalyzer::setFFTOrder(int newFFTOrder)
{
    if (newFFTOrder == fftOrder) return; // No change
    if (worker) stopWorker(); // Stop worker while we change the parameter

    fftOrder = newFFTOrder;
    isPrepared.store(false);
}

void AudioAnalyzer::setNumCQTBins(int newNumCQTBins)
{
    if (newNumCQTBins == numCQTbins) return; // No change
    if (worker) stopWorker(); // Stop worker while we change the parameter

    numCQTbins = newNumCQTBins;
    isPrepared.store(false);
}

void AudioAnalyzer::setMinFrequency(float newMinFrequency)
{
    if (std::fabs(newMinFrequency - minCQTfreq) < 1e-6) return; // No change
    if (worker) stopWorker(); // Stop worker while we change the parameter

    minCQTfreq = newMinFrequency;
    isPrepared.store(false);
}

void AudioAnalyzer::setMaxAmplitude(float newMaxAmplitude)
{
    maxAmplitude = newMaxAmplitude;
    fftScaleFactor = 4.0f / fftSize / maxAmplitude;
    cqtScaleFactor = fftScaleFactor / 28.0f;
}

void AudioAnalyzer::setThreshold(float newThreshold)
{
    threshold = newThreshold;
}

void AudioAnalyzer::setFreqWeighting(FrequencyWeighting newFreqWeighting)
{
    if (newFreqWeighting == freqWeighting) return; // No change
    if (worker) stopWorker(); // Stop worker while we change the parameter

    freqWeighting = newFreqWeighting;
    isPrepared.store(false);
}

//=============================================================================
/*  Generates A-weighting factors for the given frequencies in 'freqs' and
    stores them in 'weights'. f1, f2, f3, f4 are the constants defined in the
    A-weighting standard (IEC 61672:2003). The full formula is given in
    https://en.wikipedia.org/wiki/A-weighting#A.
*/
void AudioAnalyzer::setupAWeights(const std::vector<float>& freqs,
                                  std::vector<float>& weights)
{
    weights.resize(freqs.size());

    const float f1 = 20.598997;  // Hz
    const float f2 = 107.65265;  // Hz
    const float f3 = 737.86223;  // Hz
    const float f4 = 12194.217;  // Hz 

    for (int b = 0; b < freqs.size(); ++b)
    {
        float f = freqs[b];
        float fSquared = f * f;
        float numerator = pow(f4, 2) * pow(fSquared, 2);
        float denominator = (fSquared + pow(f1, 2)) 
                          * sqrt((fSquared + pow(f2, 2)) 
                               * (fSquared + pow(f3, 2))) 
                          * (fSquared + pow(f4, 2));

        float aWeightDB = 20.0 * log10(numerator / denominator) + 2.0; 
        float linearGain = pow(10.0, aWeightDB / 20.0); 

        // DBG("A-weight at " << f << " Hz: " << linearGain);
        weights[b] = linearGain;
    }
}

void AudioAnalyzer::setupCQT()
{
    // Concise name for pi variable
    float pi = MathConstants<float>::pi;

    // Set frequency from min to Nyquist
    const float nyquist = static_cast<float>(sampleRate * 0.5);
    const float logMin = std::log2(minCQTfreq);
    const float logMax = std::log2(nyquist);

    // Prepare CQT kernels
    cqtKernels.resize(numCQTbins);
    centerFrequencies.resize(numCQTbins);
    fullCQTspec.resize(2);
    for (int ch = 0; ch < 2; ++ch)
    {
        fullCQTspec[ch].resize(numCQTbins); // Resize vector inside array

        for (int bin = 0; bin < numCQTbins; ++bin)
        {
            fullCQTspec[ch][bin].resize(fftSize);
        }
    }

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
    }

    // Estimate memory use for CQT kernels
    // auto bytes = numCQTbins * fftSize * sizeof(std::complex<float>);
    // DBG("Estimated CQT kernel memory use: " << (bytes / 1024.0f) << " KB");
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
    juce::ignoreUnused(buffer); // Not used here. I'll take it out sometime.

    for (int ch = 0; ch < 2; ++ch)
    {
        cqtMags[ch].resize(numCQTbins, 0.0f);
        
        jassert(ffts[ch].size() == fftSize);

        // Compute CQT by inner product with each kernel
        for (int bin = 0; bin < numCQTbins; ++bin)
        {
            jassert(bin < cqtKernels.size());
            jassert(fullCQTspec[ch][bin].size() == fftSize);

            std::complex<float> sum = 0.0f;
            const auto& kernel = cqtKernels[bin];
            for (int i = 0; i < fftSize; ++i)
            {
                fullCQTspec[ch][bin][i] = ffts[ch][i] * std::conj(kernel[i]);
                sum += fullCQTspec[ch][bin][i];
            }

            cqtMags[ch][bin] = std::abs(sum);
        }
    }
}

/*  Complutes the  interaural time difference per band. Currently only 
    works for CQT transform type. 
*/
void AudioAnalyzer::computeITDs(
    std::vector<std::vector<std::vector<std::complex<float>>>> spec,
    const std::array<std::vector<float>, 2>& cqtMags,
    int numBands,
    std::vector<float>& panIndices)
{
    panIndices.resize(numBands);
    std::vector<float> itdPerBin;
    itdPerBin.resize(numBands);

    std::vector<std::complex<float>> crossSpectrum(fftSize);
    std::vector<std::complex<float>> crossCorr(fftSize);

    for (int bin = 0; bin < numBands; ++bin)
    {
        const auto& leftBin = spec[0][bin];
        const auto& rightBin = spec[1][bin];
        float freq = centerFrequencies[bin];

        // --- GCC-PHAT ---
        float alpha = alphaForFreq(freq);

        for (int k = 0; k < fftSize; ++k)
        {
            auto R = (leftBin[k] * std::conj(rightBin[k]));
            float mag = std::abs(R);

            if (mag > 1e-8f)
            {
                // Hybrid weighting: denominator is |R|^alpha
                crossSpectrum[k] = R / std::pow(mag, alpha);
            }
            else
            {
                crossSpectrum[k] = std::complex<float>(0.0f, 0.0f);
            }
        }

        // Inverse FFT to get cross-correlation
        fft->perform(crossSpectrum.data(), crossCorr.data(), true);

        // Maximum ITD in samples
        int maxLagSamples = int(sampleRate * maxITD[bin]);

        // Find peak with interpolation
        int maxIndex = 0;
        float maxVal = -1.0f;
        int bestLag = 0;
        for (int i = 0; i < fftSize; ++i)
        {
            int lag = (i <= fftSize / 2) ? i : (i - fftSize);
            if (std::abs(lag) > maxLagSamples) continue;

            float val = std::abs(crossCorr[i]);
            if (val > maxVal)
            {
                maxVal = val;
                maxIndex = i;
                bestLag = lag;
            }
        }

        // --- Coherence check ---
        // Normalize correlation peak by total energy
        float leftEnergy = 0.0f, rightEnergy = 0.0f;
        for (int k = 0; k < fftSize; ++k)
        {
            leftEnergy  += std::norm(leftBin[k]);
            rightEnergy += std::norm(rightBin[k]);
        }
        float denom = std::sqrt(leftEnergy * rightEnergy) + 1e-12f;
        float coherence = maxVal / denom;

        // float coherenceThreshold = coherenceThresholdForFreq(freq);
        // bool valid = (coherence > coherenceThreshold);
        bool valid = (coherence > 0);


        if (valid)
        {
            // Parabolic interpolation around peak
            int leftIndex  = (int)((maxIndex + fftSize - 1) % fftSize);
            int rightIndex = (int)((maxIndex + 1) % fftSize);

            float y0 = std::abs(crossCorr[leftIndex]);
            float y1 = std::abs(crossCorr[maxIndex]);
            float y2 = std::abs(crossCorr[rightIndex]);

            float denom = (y0 - 2.0f*y1 + y2);
            float peakOffset = (std::fabs(denom) > 1e-8f)
                ? 0.5f * (y0 - y2) / denom
                : 0.0f;

            float peakIndexInterp = ((float)bestLag + peakOffset);

            // Frequency- and block-size-dependent ITD expansion for bass
            float expansion = itdExpansionForFreq(freq, cqtMags, fftSize); // returns >=1.0 for low freqs

            itdPerBin[bin] = peakIndexInterp * expansion / sampleRate;
            // itdPerBin[bin] = peakIndexInterp / sampleRate;
            // itdPerBin[bin] = (float)bestLag * expansion / sampleRate;

            panIndices[bin] = juce::jlimit(-1.0f, 1.0f, itdPerBin[bin] / maxITD[bin]);
        }
        else
        {
            if (panMethod == time_pan) // For time_pan method, set to NaN if invalid
                panIndices[bin] = std::numeric_limits<float>::quiet_NaN();
            
            else // For 'both' method, just set to zero
                panIndices[bin] = 0.0f;
        }
    }
}

//=============================================================================
/*  This function is called on the worker thread whenever a new block is
    to be analyzed. It computes the selected frequency transform and 
    panning method, and stores the results in the 'results' member 
    variable for the GUI thread to access.
*/
void AudioAnalyzer::analyzeBlock(const juce::AudioBuffer<float>& buffer)
{
    if (!isPrepared.load())
        return; // Not prepared yet

    // Temporary buffers
    std::array<std::vector<float>, 2> magnitudes;
    std::vector<float> ilds;
    std::vector<float> itds;
    std::vector<float> panIndices;
    int numBands = 0;
    float epsilon = 1e-12f;
    
    // Compute FFT for the block
    std::array<fft_buffer_t, 2> ffts;
    computeFFT(buffer, ffts);

    // Compute the selected frequency transform for the signal
    if (transform == FFT)
    {
        // Copy magnitudes from FFT results
        for (int ch = 0; ch < 2; ++ch)
        {
            magnitudes[ch].resize(numFFTBins);
            for (int b = 0; b < numFFTBins; ++b)
                magnitudes[ch][b] = std::abs(ffts[ch][b]);
        }
        numBands = numFFTBins;
    }
    else if (transform == CQT)
    {
        // Compute CQT magnitudes
        computeCQT(buffer, ffts, magnitudes);
        numBands = numCQTbins;
    }
    else
    {
        jassertfalse; // Unknown transform type
        return;
    }

    // Compute panning indices based on the selected pan method
    if (panMethod == level_pan)
    {
        // Use ILD pan indices
        computeILDs(magnitudes, numBands, panIndices);
    }
    else if (panMethod == time_pan)
    {
        // Use ITD pan indices
        computeITDs(fullCQTspec, magnitudes, numBands, panIndices);
    }
    else if (panMethod == both)
    {
        computeILDs(magnitudes, numBands, ilds);
        computeITDs(fullCQTspec, magnitudes, numBands, itds);
        panIndices.resize(numBands);

        for (int b = 0; b < numBands; b++)
        {
            panIndices[b] = (ildWeights[b] * ilds[b] 
                           + itdWeights[b] * itds[b]);

            const float extremeThreshold = 0.85f;
            if (std::abs(ilds[b]) > extremeThreshold)
                panIndices[b] = ilds[b];
            else if (std::abs(itds[b]) > extremeThreshold)
                panIndices[b] = itds[b];
        }
    }
    else
    {
        jassertfalse; // Unknown pan method
        return;
    }
    
    // Compute (estimated) perceived amplitudes and build results vector
    std::vector<frequency_band> newResults;
    newResults.reserve(numBands);

    for (int b = 0; b < numBands; ++b)
    {
        float magL = std::abs(magnitudes[0][b]);
        float magR = std::abs(magnitudes[1][b]);
        float mag = (magL + magR) * 0.5f; // Average magnitude

        // DBG("freq = " << centerFrequencies[b]
        //     << " Hz, aWeight = " << frequencyWeights[b]);
        float linear = mag * fftScaleFactor; // Linear amplitude

        if (freqWeighting != none)
            linear *= frequencyWeights[b]; // Apply frequency weighting
        
        if (transform == CQT)
            linear /= 28.f; // Additional scaling for CQT

        float dBrel = 20 * std::log10(linear / maxAmplitude + epsilon);
        if (dBrel < threshold)
            continue; // Below threshold

        float amp = (dBrel - threshold) / -threshold; // Scale to [0, 1]
        amp = juce::jlimit(0.0f, 1.0f, amp); // Clamp

        newResults.push_back({ centerFrequencies[b], amp, panIndices[b] });
    }

    // Hand results to GUI
    {
        std::lock_guard<std::mutex> resultsLock(resultsMutex);
        results = std::move(newResults);
        // DBG("Analyzed block. Results size = " << results.size());
    }
}