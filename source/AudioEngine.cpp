#include "AudioEngine.h"

//=============================================================================
AudioEngine::AudioEngine()
{
    // Tell JUCE which formats we can open
    formatManager.registerBasicFormats();
}

//=============================================================================
AudioEngine::~AudioEngine()
{
    transport.setSource(nullptr);
}

//=============================================================================
void AudioEngine::audioDeviceIOCallback(const float *const *inputChannelData,
                                        int numInputChannels,
                                        float *const *outputChannelData,
                                        int numOutputChannels,
                                        int numSamples)
{
    juce::AudioBuffer<float> buffer(numOutputChannels, numSamples);
    juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);

    switch (inputType)
    {
        case file:
            // Fill the buffer with audio data from the transport
            transport.getNextAudioBlock(info);

            // Copy the output data to the output channels
            for (int ch = 0; ch < numOutputChannels; ++ch)
                std::memcpy(outputChannelData[ch],
                            buffer.getReadPointer(ch),
                            sizeof(float) * (size_t)numSamples);
            break;

        case streaming:
            DBG("Streaming input type selected, passing through input data directly.");
            // Pass through the input data directly to output
            for (int ch = 0; ch < numOutputChannels; ++ch)
                std::memcpy(outputChannelData[ch],
                            inputChannelData[ch],
                            sizeof(float) * (size_t)numSamples);
            break;
    }
}

bool AudioEngine::loadFile(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);
    if (reader == nullptr) return false; // unsupported / unreadable

    auto newSource = std::make_unique<juce::AudioFormatReaderSource>
                                            (reader, /* readerOwned */ true);

    transport.stop();
    transport.setSource(newSource.get(),
                        0, // read-ahead (use default)
                        nullptr, // no background thread yet
                        reader->sampleRate);

    fileSource = std::move(newSource);

    transport.setPosition(0);
    transport.start();
    return true;
}

void AudioEngine::togglePlayback()
{
    if (transport.isPlaying())
        transport.stop();
    else
        transport.start();
}

//=============================================================================
void AudioEngine::setInputType(InputType type)
{
    this->inputType = type;

    switch (inputType)
    {
    case file:
        transport.setPosition (0.0);
        transport.start();
        break;
    
    case streaming:
        transport.stop();
        break;
    }
}

bool AudioEngine::isPlaying() const
{
    return transport.isPlaying();
}

void AudioEngine::stopPlayback()
{
    transport.stop();
}

void AudioEngine::startPlayback()
{
    transport.start();
}

//=============================================================================
void AudioEngine::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    transport.prepareToPlay(samplesPerBlock, sampleRate);
}

void AudioEngine::releaseResources()
{
    transport.releaseResources();
}