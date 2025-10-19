#include "AudioAnalyzer.h"

//=============================================================================
AudioAnalyzer::AudioAnalyzer() {}
AudioAnalyzer::~AudioAnalyzer()
{
    // Destroy worker, which joins the thread
    for (auto& worker : workers)
    {
        stopWorker(worker);
    }
}

//=============================================================================
/*  Prepares the audio analyzer. */
void AudioAnalyzer::prepare(double newSampleRate, int numTracks)
{
    if (isPrepared.load())
        return; // Already prepared

    for (auto& worker : workers)
    {
        stopWorker(worker); // Stop any existing worker thread
    }

    sampleRate = newSampleRate;
    
    numFFTBins = windowSize / 2 + 1;

    setScaleFactors(windowSize, maxAmplitude, cqtNormalization);

    // Initialize FFT engine and storage (if needed)
    if (fftEngine == nullptr || fftEngine->getSize() != windowSize)
        fftEngine = std::make_unique<juce::dsp::FFT>((int)std::log2(windowSize));
    
    if (transform == FFT)
        numBands = numFFTBins;
    else if (transform == CQT)
        numBands = numCQTbins;

    // Initialize results storage
    fftSpectra[0].resize(windowSize);
    fftSpectra[1].resize(windowSize);
    fftData.resize(2 * windowSize);
    magnitudes[0].resize(numBands);
    magnitudes[1].resize(numBands);
    binFrequencies.resize(numBands);
    ilds.resize(numBands);
    itds.resize(numBands);
    panIndices.resize(numBands);
    newResults.reserve(numBands);

    // Initialize members needed for the selected frequency transform
    if (transform == FFT)
        setupFFT();
    else if (transform == CQT)
        setupCQT();
    
    // Build the Hann window (if needed)
    if (window.size() != windowSize)
    {
        window.resize(windowSize);
        for (int n = 0; n < windowSize; ++n)
            window[n] = 0.5f * (1.0f - std::cos(2.0f * pi * n / (windowSize - 1)));
    }

    // Set up frequency-weighting factors
    if (freqWeighting == A_weighting)
        setupAWeights(binFrequencies, frequencyWeights);

    // Compute frequency-dependent ITD/ILD weights
    if (panMethod == both || panMethod == time_pan)
        setupPanWeights();

    // Start the worker threads
    for (int i = 0; i < numTracks; ++i)
    {
        auto worker = std::make_unique<AnalyzerWorker>(windowSize, hopSize, sampleRate, i, *this);
        worker->start();
        workers.push_back(std::move(worker));
    }

    isPrepared.store(true);
}

void AudioAnalyzer::prepare()
{
    // Use current sampleRate if none specified
    prepare(sampleRate);
}

void AudioAnalyzer::enqueueBlock(const juce::AudioBuffer<float>* buffer, int trackIndex, int numTracks)
{
    if (!buffer) return;

    if (numTracks != workers.size())
    {
        pendingNumTracks.store(numTracks);
        flushAnalysisQueue();  // drop old blocks waiting in queue
    }

    if (trackIndex < workers.size() && workers[trackIndex])
        workers[trackIndex]->pushBlock(*buffer);
}


void AudioAnalyzer::stopWorker(std::unique_ptr<AnalyzerWorker>& worker)
{
    if (worker != nullptr)
    {
        worker->stop();    // Stop cleanly
        worker.reset();    // Then delete
    }
}

void AudioAnalyzer::flushAnalysisQueue()
{
    for (auto& worker : workers)
    {
        if (worker)
            worker->flushRingBuffer();
    }
}

std::vector<frequency_band> AudioAnalyzer::getLatestResults() const
{
    std::lock_guard<std::mutex> lock(resultsMutex);
    return results;
}

void AudioAnalyzer::setWindowSize(int newWindowSize)
{
    if (newWindowSize == windowSize) 
        return; // No change

    for (auto& worker : workers)
    {
        stopWorker(worker);
    }

    for (auto& worker : workers)
    {
        if (worker != nullptr) 
            stopWorker(worker); // Stop the worker thread while we change the parameter
    }

    windowSize = newWindowSize;
    isPrepared.store(false);
}

void AudioAnalyzer::setHopSize(int newHopSize)
{
    if (newHopSize == hopSize) 
        return; // No change

    hopSize = newHopSize;

    for (auto& worker : workers)
    {
        if (worker != nullptr) 
            worker->setHopSize(newHopSize);
    }
}

void AudioAnalyzer::setTransform(Transform newTransform)
{
    if (newTransform == transform) 
        return; // No change
    
    for (auto& worker : workers)
    {
        stopWorker(worker); // Stop worker while we change the parameter
    }

    transform = newTransform;
    isPrepared.store(false);
}

void AudioAnalyzer::setPanMethod(PanMethod newPanMethod)
{
    if (newPanMethod == panMethod) 
        return; // No change

    for (auto& worker : workers)
    {
        stopWorker(worker); // Stop worker while we change the parameter
    }

    panMethod = newPanMethod;
    isPrepared.store(false);
}

void AudioAnalyzer::setNumCQTBins(int newNumCQTBins)
{
    if (newNumCQTBins == numCQTbins) 
        return; // No change

    for (auto& worker : workers)
    {
        stopWorker(worker); // Stop worker while we change the parameter
    }

    numCQTbins = newNumCQTBins;
    isPrepared.store(false);
}

void AudioAnalyzer::setMinFrequency(float newMinFrequency)
{
    if (std::abs(newMinFrequency - minCQTfreq) < 1e-6f) 
        return; // No change

    for (auto& worker : workers)
    {
        stopWorker(worker); // Stop worker while we change the parameter
    }

    minCQTfreq = newMinFrequency;
    isPrepared.store(false);
}

void AudioAnalyzer::setMaxAmplitude(float newMaxAmplitude)
{
    maxAmplitude = newMaxAmplitude;
    setScaleFactors(windowSize, maxAmplitude, cqtNormalization);
}

void AudioAnalyzer::setThreshold(float newThreshold)
{
    threshold = newThreshold;
}

void AudioAnalyzer::setFreqWeighting(FrequencyWeighting newFreqWeighting)
{
    if (newFreqWeighting == freqWeighting) return; // No change

    for (auto& worker : workers)
    {
        stopWorker(worker); // Stop worker while we change the parameter
    }

    freqWeighting = newFreqWeighting;
    isPrepared.store(false);
}

//=============================================================================
void AudioAnalyzer::setScaleFactors(int windowSizeIn, 
                                    float maxAmplitudeIn, 
                                    float cqtNormalizationIn)
{
    fftScaleFactor = 4.0f / windowSizeIn / maxAmplitudeIn;
    cqtScaleFactor = fftScaleFactor * cqtNormalizationIn;
}

/*  Initializes the variables and vectors needed for FFT mode, which is
    just binFrequencies in this case.
*/
void AudioAnalyzer::setupFFT()
{
    const float binWidth = ((float)sampleRate / 2.f) / numFFTBins;
    for (int b = 0; b < numBands; ++b)
    {
        binFrequencies[b] = b * binWidth;
    }
}

/*  Initializes the variables and vectors needed for CQT mode, including 
    binFrequencies, cqtKernels, and fullCQTspec. 
*/
void AudioAnalyzer::setupCQT()
{
    // Prepare CQT kernels
    cqtKernels.resize(numBands);
    
    for (int ch = 0; ch < 2; ++ch)
    {
        fullCQTspec[ch].resize(numBands); // Resize vector inside array

        for (int bin = 0; bin < numBands; ++bin)
        {
            fullCQTspec[ch][bin].resize(windowSize);
        }
    }

    // Set frequency from min to Nyquist
    const float nyquist = static_cast<float>(sampleRate * 0.5);
    const float logMin = std::log2(minCQTfreq);
    const float logMax = std::log2(nyquist);

    // Precompute CQT kernels
    for (int bin = 0; bin < numBands; ++bin)
    {
        // Compute center frequency for this bin
        float frac = bin * 1.0f / (numBands + 1);
        float freq = std::pow(2.0f, logMin + frac * (logMax - logMin));
        binFrequencies[bin] = freq;

        // Generate complex sinusoid for this frequency (for inner products)
        int kernelLength = windowSize;
        std::vector<std::complex<float>> kernelTime(kernelLength, 0.0f);
        
        for (int n = 0; n < kernelLength; ++n)
        {
            // Edited for centering
            float t = (n - windowSize * 0.5f) / float(sampleRate); 
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
    // auto bytes = numCQTbins * windowSize * sizeof(std::complex<float>);
    // DBG("Estimated CQT kernel memory use: " << (bytes / 1024.0f) << " KB");
}

/*  Generates A-weighting factors for the given frequencies in 'freqs' 
    and stores them in 'weights'. f1, f2, f3, f4 are the constants 
    defined in the A-weighting standard (IEC 61672:2003). The full 
    formula is given in https://en.wikipedia.org/wiki/A-weighting#A.
*/
void AudioAnalyzer::setupAWeights(const std::vector<float>& freqs,
                                  std::vector<float>& weights)
{
    weights.resize(freqs.size());

    const float f1 = 20.598997f;  // Hz
    const float f2 = 107.65265f;  // Hz
    const float f3 = 737.86223f;  // Hz
    const float f4 = 12194.217f;  // Hz

    for (int b = 0; b < freqs.size(); ++b)
    {
        float f = freqs[b];
        float fSquared = f * f;
        float numerator = pow(f4, 2.0f) * pow(fSquared, 2.0f);
        float denominator = (fSquared + pow(f1, 2.0f)) 
                          * sqrt((fSquared + pow(f2, 2.0f)) 
                               * (fSquared + pow(f3, 2.0f))) 
                          * (fSquared + pow(f4, 2.0f));

        float aWeightDB = 20.0f * log10(numerator / denominator) + 2.0f; 
        float linearGain = pow(10.0f, aWeightDB / 20.0f); 

        weights[b] = linearGain;
    }
}

void AudioAnalyzer::setupPanWeights()
{
    itdWeights.resize(numBands);
    ildWeights.resize(numBands);
    itdPerBin.resize(numBands);
    maxITD.resize(numBands);

    for (int bin = 0; bin < numBands; ++bin)
    {
        // Best-fit curve
        float ITDweight = 1.0f / (1.0f + std::pow(binFrequencies[bin] / f_trans, p));
        float ILDweight = 1.0f - ITDweight;

        itdWeights[bin] = ITDweight;
        ildWeights[bin] = ILDweight;

        // Smooth exponential decay from ITD_low to ITD_high
        maxITD[bin] = maxITDhigh + (maxITDlow - maxITDhigh) 
                                 * std::exp(-binFrequencies[bin] / 2000.0f);
    } 
}

//=============================================================================
/*  This function is called on the worker thread whenever a new block is
    to be analyzed. It computes the selected frequency transform and 
    panning method, and stores the results in the 'results' member 
    variable for the GUI thread to access.
*/
void AudioAnalyzer::analyzeBlock(const juce::AudioBuffer<float>& buffer, int trackIndex)
{
    newNumTracks = pendingNumTracks.load();
    if (newNumTracks > 0)
    {
        isPrepared.store(false);
        prepare(sampleRate, newNumTracks);
        pendingNumTracks.store(-1);
    }

    if (!isPrepared.load())
        return; // Not prepared yet
    
    // Compute FFT for the block
    computeFFT(buffer, window, fftData, fftSpectra);

    // Compute the selected frequency transform for the signal
    if (transform == FFT)
    {
        // Copy magnitudes from FFT results
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int b = 0; b < numFFTBins; ++b)
                magnitudes[ch][b] = std::abs(fftSpectra[ch][b]);
        }
    }
    else if (transform == CQT)
    {
        // Compute CQT magnitudes
        computeCQT(fftSpectra, cqtKernels, fullCQTspec, magnitudes);
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
        computeILDs(magnitudes, panIndices);
    }
    else if (panMethod == time_pan)
    {
        // Use ITD pan indices
        computeITDs(fullCQTspec, numBands, panIndices);
    }
    else if (panMethod == both)
    {
        computeILDs(magnitudes, ilds);
        computeITDs(fullCQTspec, numBands, itds);

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
    newResults.clear();

    for (int b = 0; b < numBands; ++b)
    {
        float magL = std::abs(magnitudes[0][b]);
        float magR = std::abs(magnitudes[1][b]);
        float mag = (magL + magR) * 0.5f; // Average magnitude
        float linear = mag; // Linear amplitude

        if (transform == FFT)
            linear *= fftScaleFactor; // Linear amplitude
        else if (transform == CQT)
            linear *= cqtScaleFactor;

        if (freqWeighting != none)
            linear *= frequencyWeights[b]; // Apply frequency weighting

        float dBrel = 20 * std::log10(linear + epsilon);
        if (dBrel < threshold)
            continue; // Below threshold

        float amp = (dBrel - threshold) / -threshold; // Scale to [0, 1]
        amp = juce::jlimit(0.0f, 1.0f, amp); // Clamp

        newResults.push_back({ binFrequencies[b], amp, panIndices[b], trackIndex });
    }

    // Hand results to GUI
    {
        std::lock_guard<std::mutex> resultsLock(resultsMutex);
        results = std::move(newResults);
    }

    // DBG("Analyzed block. Results size = " << results.size());
}

/*  Computes the FFT of each channel of the input buffer and stores the
    results in outSpectra.
*/
void AudioAnalyzer::computeFFT(const juce::AudioBuffer<float>& buffer,
                               const std::vector<float>& windowIn,
                               std::vector<float>& fftDataTemp,
                               std::array<std::vector<Complex>, 2>& outSpectra)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        // Copy & window the buffer data
        auto* readPtr = buffer.getReadPointer(ch);
        for (int n = 0; n < windowSize; ++n)
        {
            fftDataTemp[n] = readPtr[n] * windowIn[n];
        }
            
        // Compute an in-place FFT
        fftEngine->performRealOnlyForwardTransform(fftDataTemp.data());

        // Copy the results to the output 
        for (int b = 0; b < windowSize; ++b)
        {
            outSpectra[ch][b].real(fftDataTemp[2 * b]);
            outSpectra[ch][b].imag(fftDataTemp[2 * b + 1]);
        }
    }
}

/*  Computes the CQT of an audio buffer given the FFT results and stores
    the magnitudes (one for each channel and CQT bin) in cqtMags.
*/
void AudioAnalyzer::computeCQT(const std::array<std::vector<Complex>, 2>& ffts,
                               const std::vector<std::vector<Complex>>& cqtKernelsIn,
                               std::array<std::vector<std::vector<std::complex<float>>>, 2>& spectraOut,
                               std::array<std::vector<float>, 2>& magnitudesOut)
{
    for (int ch = 0; ch < 2; ++ch)
    {        
        jassert(ffts[ch].size() == windowSize);

        // Compute CQT by inner product with each kernel
        for (int bin = 0; bin < magnitudesOut[ch].size(); ++bin)
        {
            jassert(bin < cqtKernels.size());
            jassert(spectraOut[ch][bin].size() == windowSize);

            std::complex<float> sum = 0.0f;
            const auto& kernel = cqtKernels[bin];
            for (int i = 0; i < windowSize; ++i)
            {
                spectraOut[ch][bin][i] = ffts[ch][i] * std::conj(kernel[i]);
                sum += spectraOut[ch][bin][i];
            }

            magnitudesOut[ch][bin] = std::abs(sum);
        }
    }
}

/*  Computes the inter-channel level difference for each frequency bin
    and stores the results in panIndices.
*/
void AudioAnalyzer::computeILDs(const std::array<std::vector<float>, 2>& magnitudesIn,
                                std::vector<float>& panOut)
{
    for (int b = 0; b < magnitudesIn[0].size(); ++b)
    {
        float L = magnitudesIn[0][b];
        float R = magnitudesIn[1][b];
        float denom = L * L + R * R + epsilon; // Avoid zero
        float sim = (2.0f * L * R) / denom;

        // Direction: L > R => left => -1; R > L => right => +1
        float dir = (L > R ? -1.0f : (R > L ? 1.0f : 0.0f));

        // Final pan is in the range [-1, +1]
        panOut[b] = dir * (1.0f - sim);
    }
}

/*  Complutes the  interaural time difference per band. Currently only 
    works for CQT transform type. 
*/
void AudioAnalyzer::computeITDs(
    const std::array<std::vector<std::vector<std::complex<float>>>, 2>& spec,
    int numBands,
    std::vector<float>& panIndices)
{
    std::vector<std::complex<float>> crossSpectrum(windowSize);
    std::vector<std::complex<float>> crossCorr(windowSize);

    for (int bin = 0; bin < numBands; ++bin)
    {
        const auto& leftBin = spec[0][bin];
        const auto& rightBin = spec[1][bin];
        float freq = binFrequencies[bin];

        // --- GCC-PHAT ---
        float alpha = alphaForFreq(freq);

        for (int k = 0; k < windowSize; ++k)
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
        fftEngine->perform(crossSpectrum.data(), crossCorr.data(), true);

        // Maximum ITD in samples
        int maxLagSamples = int(sampleRate * maxITD[bin]);

        // Find peak with interpolation
        int maxIndex = 0;
        float maxVal = -1.0f;
        int bestLag = 0;
        for (int i = 0; i < windowSize; ++i)
        {
            int lag = (i <= windowSize / 2) ? i : (i - windowSize);
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
        for (int k = 0; k < windowSize; ++k)
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
            int leftIndex  = (int)((maxIndex + windowSize - 1) % windowSize);
            int rightIndex = (int)((maxIndex + 1) % windowSize);

            float y0 = std::abs(crossCorr[leftIndex]);
            float y1 = std::abs(crossCorr[maxIndex]);
            float y2 = std::abs(crossCorr[rightIndex]);

            denom = (y0 - 2.0f * y1 + y2);
            float peakOffset = (std::fabs(denom) > 1e-8f)
                ? 0.5f * (y0 - y2) / denom
                : 0.0f;

            float peakIndexInterp = ((float)bestLag + peakOffset);

            itdPerBin[bin] = peakIndexInterp / (float)sampleRate;

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
/* What is alpha? */
float AudioAnalyzer::alphaForFreq(float f)
{
    // log-scale (Hz)
    float logf = std::log10(f + 1.0f);   // +1 to avoid log(0)
    float logLow = std::log10(100.0f);   // anchor: 100 Hz
    float logHigh = std::log10(4000.0f); // anchor: 4 kHz

    // Normalize [0..1] across range
    float t = juce::jlimit(0.0f, 1.0f, (logf - logLow) / (logHigh - logLow));

    // Interpolate smoothly between 0.8 and 1.0
    return 0.8f + t * (1.0f - 0.4f);
}

float AudioAnalyzer::coherenceThresholdForFreq(float f)
{
    float logf = std::log10(f + 1.0f);
    float logLow = std::log10(100.0f);
    float logHigh = std::log10(4000.0f);

    float t = juce::jlimit(0.0f, 1.0f, (logf - logLow) / (logHigh - logLow));

    // Threshold from ~1e-7 at lows → ~1e-6 at highs
    float lowThresh = 1e-7f;
    float highThresh = 1e-6f;

    return lowThresh * std::pow((highThresh / lowThresh), t); // geometric interpolation
}