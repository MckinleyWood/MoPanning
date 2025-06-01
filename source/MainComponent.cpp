#include "MainComponent.h"

//======================================================================
// Helper functions

inline float hzToMel(float hz) { return 2595.0f * std::log10(1.0f + hz / 700.0f); }
inline float melToHz(float mel) { return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f); }

std::vector<std::vector<float>> createMelFilterbank(int numFilters, int fftSize, float sampleRate, float minHz = 0.0f, float maxHz = -1.0f)
{
    if (maxHz < 0.0f) maxHz = sampleRate / 2.0f;
    std::vector<std::vector<float>> filterbank(numFilters, std::vector<float>(fftSize / 2, 0.0f));

    float minMel = hzToMel(minHz);
    float maxMel = hzToMel(maxHz);

    std::vector<float> melPoints(numFilters + 2);
    for (int i = 0; i < melPoints.size(); ++i)
    {
        melPoints[i] = melToHz(minMel + (maxMel - minMel) * i / (numFilters + 1));
    }

    std::vector<int> bin(melPoints.size());
    for (int i = 0; i < bin.size(); ++i)
    {
        bin[i] = static_cast<int>(std::floor((fftSize + 1) * melPoints[i] / sampleRate));
    }

    for (int m = 1; m <= numFilters; ++m)
    {
        for (int k = bin[m - 1]; k < bin[m]; ++k)
            if (k < filterbank[m - 1].size())
                filterbank[m - 1][k] = (k - bin[m - 1]) / float(bin[m] - bin[m - 1]);

        for (int k = bin[m]; k < bin[m + 1]; ++k)
            if (k < filterbank[m - 1].size())
                filterbank[m - 1][k] = (bin[m + 1] - k) / float(bin[m + 1] - bin[m]);
    }
    return filterbank;
}

std::vector<float> dct(const std::vector<float>& input, int numCoeffs)
{
    int N = input.size();
    std::vector<float> result(numCoeffs, 0.0f);
    for (int k = 0; k < numCoeffs; ++k)
    {
        for (int n = 0; n < N; ++n)
            result[k] += input[n] * std::cos(M_PI * k * (2 * n + 1) / (2 * N));
    }
    return result;
}


//======================================================================
MainComponent::MainComponent()
{
    setSize (600, 400);
    analysisBuffer.setSize(2, analysisBufferSize);
    analysisBuffer.clear();
    analysisBufferWritePos = 0;

    startTimerHz(30);
}

//======================================================================
void MainComponent::paint (juce::Graphics& g)
{
    /* Our component is opaque, so we must completely fill the 
    background with a solid colour. */
    g.fillAll (getLookAndFeel().findColour(
        juce::ResizableWindow::backgroundColourId));

    g.setFont (juce::FontOptions (16.0f));
    g.setColour (juce::Colours::white);
    g.drawText ("Hi Owen", getLocalBounds(), 
        juce::Justification::centred, true);
}

void MainComponent::resized()
{
    /* This is called when the MainComponent is resized. If you add any 
    child components, this is where you should update their positions.
    */
}

void MainComponent::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                        int numInputChannels,
                                        float* const* outputChannelData,
                                        int numOutputChannels,
                                        int numSamples,
                                        const juce::AudioIODeviceCallbackContext& context)
{
    if(numInputChannels >=2) {
        for (int i = 0; i < numSamples; ++i) {
            analysisBuffer.setSample(0, analysisBufferWritePos, inputChannelData[0][i]);
            analysisBuffer.setSample(1, analysisBufferWritePos, inputChannelData[1][i]);
            analysisBufferWritePos = (analysisBufferWritePos + 1) % analysisBufferSize;
        }
    }
    
    juce::ignoreUnused(outputChannelData, numOutputChannels, context);
}

void MainComponent::timerCallback()
{
    // Copy a frame from the circular buffer for each channel
    std::vector<float> left(fftSize), right(fftSize);

    int readPos = (analysisBufferWritePos + analysisBufferSize - fftSize) % analysisBufferSize;
    for (int i = 0; i < fftSize; ++i) {
        left[i]  = analysisBuffer.getSample(0, (readPos + i) % analysisBufferSize);
        right[i] = analysisBuffer.getSample(1, (readPos + i) % analysisBufferSize);
    }

    // Apply window
    window.multiplyWithWindowingTable(left.data(), fftSize);
    window.multiplyWithWindowingTable(right.data(), fftSize);

    // Prepare fft buffers (real + imag interleaved)
    juce::HeapBlock<float> fftDataL(2 * fftSize, true);
    juce::HeapBlock<float> fftDataR(2 * fftSize, true);
    std::copy(left.begin(), left.end(), fftDataL.get());
    std::copy(right.begin(), right.end(), fftDataR.get());

    // Perform FFT on the buffer
    fft.performFrequencyOnlyForwardTransform(fftDataL.get());
    fft.performFrequencyOnlyForwardTransform(fftDataR.get());

    // Compute magnitude spectrum for both channels
    std::vector<float> magL(fftSize / 2), magR(fftSize / 2);
    for (int i = 0; i < fftSize / 2; ++i)
    {
        float reL = fftDataL[i * 2];
        float imL = fftDataL[i * 2 + 1];
        float reR = fftDataR[i * 2];
        float imR = fftDataR[i * 2 + 1];
        magL[i] = std::sqrt(reL * reL + imL * imL);
        magR[i] = std::sqrt(reR * reR + imR * imR);
    }

    // Parameters
    constexpr int numMelBands = 20;
    constexpr int numMfccs = 13;
    float sampleRate = 44100.0f;

    // Create Mel filterbank once (static for efficiency)
    static auto melFilterbank = createMelFilterbank(numMelBands, fftSize, sampleRate);

    // Apply Mel filterbank to magnitude spectra
    std::vector<float> melEnergiesL(numMelBands, 0.0f), melEnergiesR(numMelBands, 0.0f);
    for (int m = 0; m < numMelBands; ++m)
    {
        for (int k = 0; k < fftSize / 2; ++k)
        {
            melEnergiesL[m] += magL[k] * melFilterbank[m][k];
            melEnergiesR[m] += magR[k] * melFilterbank[m][k];
        }
        melEnergiesL[m] = std::log(melEnergiesL[m] + 1e-8f); // Add small value to avoid log(0)
        melEnergiesR[m] = std::log(melEnergiesR[m] + 1e-8f);
    }

    // DCT to get MFCCs
    std::vector<float> mfccL = dct(melEnergiesL, numMfccs);
    std::vector<float> mfccR = dct(melEnergiesR, numMfccs);
    // Now mfccL and mfccR contain the MFCCs for the left and right channels


    // Compute panning index **I need to double check this cause its 
    // AI generated and I dont know if it know panning index as well as it does mfccs**
    std::vector<float> panningIndex(numMfccs);
    for (int i = 0; i < numMfccs; ++i)
        panningIndex[i] = (mfccL[i] - mfccR[i]) / (std::abs(mfccL[i]) + std::abs(mfccR[i]) + 1e-8f);
}

void MainComponent::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    juce::ignoreUnused(device);
}

void MainComponent::audioDeviceStopped()
{

}