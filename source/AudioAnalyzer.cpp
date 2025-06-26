#include "AudioAnalyzer.h"

//=============================================================================
AudioAnalyzer::AudioAnalyzer() {}
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
void AudioAnalyzer::prepare()
{
    // Initialize AnalyzerWorker
    DBG("Preparing AudioAnalyzer...");
    int numSlots = 4;
    int samplesPerBlock = 512;
    worker = std::make_unique<AnalyzerWorker>(numSlots, samplesPerBlock, *this);
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

//=============================================================================
/*  Analyze a block of audio data
*/
void AudioAnalyzer::analyzeBlock(const juce::AudioBuffer<float>& buffer)
{
    // Calculate RMS amplitude
    int numSamples = buffer.getNumSamples();
    double sumSquares = 0.0;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            sumSquares += data[i] * data[i];
    }

    float rms = std::sqrt(sumSquares / numSamples / 2);
    frequency_band band = { 1000.f, rms, 0.5f };

    std::lock_guard<std::mutex> lock(resultsMutex);
    results.clear();
    results.push_back(band);
}