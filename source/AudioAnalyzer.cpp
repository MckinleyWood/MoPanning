#include "AudioAnalyzer.h"

//=============================================================================
AudioAnalyzer::AudioAnalyzer() {}
AudioAnalyzer::~AudioAnalyzer()
{
    // Destroy worker, which joins the thread
    worker.reset();
}

//=============================================================================
/*  
*/
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

void AudioAnalyzer::computeILDs(const std::vector<float>& magsL,
                                const std::vector<float>& magsR,
                                std::vector<float>& outPan)
{
    int numBins = (int)magsL.size();
    outPan.resize(numBins);

    for (int b = 0; b < numBins; ++b)
    {
        // This is just chatGPT I don't know if its panning index ala marpan

        float L = magsL[b];
        float R = magsR[b];
        float denom = L*L + R*R + 1e-12f; // Avoid zero
        float sim = (2.0f * L * R) / denom;

        // Direction: L > R => left => -1; R > L => right => +1
        float dir = (L > R ? -1.0f : (R > L ? 1.0f : 0.0f));

        // Final pan is in the range [-1, +1]
        outPan[b] = dir * (1.0f - sim);
    }
}

/* Analyze a block of audio data */
void AudioAnalyzer::analyzeBlock(const juce::AudioBuffer<float>& buffer)
{
    // DBG("Analyzing the next audio block");

    // FFT & get complex spectra
    std::array<fft_buffer_t, 2> spectra;
    computeFFT(buffer, spectra);

    // Compute magnitudes per channel
    std::vector<float> magsL(numBins), magsR(numBins);
    for (int b = 0; b < numBins; ++b)
    {
        magsL[b] = std::abs(spectra[0][b]);
        magsR[b] = std::abs(spectra[1][b]);
    }

    // Compute ILD pan indices
    std::vector<float> panIndex;
    computeILDs(magsL, magsR, panIndex);

    // Build frequency_band list
    std::vector<frequency_band> newResults;
    newResults.reserve(numBins);

    float binWidth = (float)sampleRate / (float)fftSize;
    for (int b = 0; b < numBins; ++b)
    {
        float freq = b * binWidth;
        float amp  = (magsL[b] + magsR[b]) * 0.5f; // Average amplitude
        amp = juce::jlimit(0.0f, 1.0f, amp / maxExpectedMag);
        amp = sqrt(amp);
        newResults.push_back({ freq, amp, panIndex[b] });
    }

    // Publish to GUI
    std::lock_guard<std::mutex> lock(resultsMutex);
    results = std::move(newResults);
}