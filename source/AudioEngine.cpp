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
void AudioEngine::fillAudioBuffers(const float *const *inputChannelData,
                                   int numInputChannels,
                                   float *const *outputChannelData,
                                   int numOutputChannels,
                                   int numSamples,
                                   juce::AudioBuffer<float>& buffer)
{
    buffer.clear();
    juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);

    // Fill the analysis/intermediate buffer from the specified input
    switch (inputType)
    {
        case file:
            // Fill the buffer with audio data from the transport
            transport.getNextAudioBlock(info);

            break;

        case streaming:
            // Fill the buffer with the input data directly
            if (numInputChannels >= 1)
                buffer.copyFrom(0, 0, inputChannelData[0], numSamples);
            if (numInputChannels >= 2)
                buffer.copyFrom(1, 0, inputChannelData[1], numSamples);
            
            break;
    }

    // Copy the filled buffer to the output channels
    if (numOutputChannels == 1)
    {
        // Downmix to mono if only one output channel
        static std::vector<float> temp;
        temp.resize(numSamples);
        const float *left = buffer.getReadPointer(0);
        const float *right = buffer.getReadPointer(1);

        for (int i = 0; i < numSamples; ++i)
            temp[i] = (left[i] + right[i]) * 0.5f;
            
        std::memcpy(outputChannelData[0], temp.data(),
                    sizeof(float) * numSamples);
    }
    else
    {
        // Else copy the buffer to the first two output channels
        std::memcpy(outputChannelData[0],
                    buffer.getReadPointer(0),
                    sizeof(float) * numSamples);
        std::memcpy(outputChannelData[1],
                    buffer.getReadPointer(1),
                    sizeof(float) * numSamples);
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