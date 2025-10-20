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
void AudioEngine::fillAudioBuffers(const float *const *inputChannelData, int numInputChannels,
                                   float *const *outputChannelData, int numOutputChannels,
                                   int numSamples, juce::AudioBuffer<float>& buffer, bool isFirstTrack, float trackGainIn)
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
            {
                jassert (buffer.getNumChannels() >= 2);
                DBG("fillAudioBuffers: buffer channels = " << buffer.getNumChannels()
                    << " numSamples = " << numSamples);
                buffer.copyFrom(0, 0, inputChannelData[0], numSamples);
            }
            if (numInputChannels >= 2)
                buffer.copyFrom(1, 0, inputChannelData[1], numSamples);
            
            break;
    }

    const float* inL = buffer.getReadPointer(0);
    const float* inR = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr;

    // Copy the filled buffer to the output channels
    if (numOutputChannels == 1)
    {
        float* out = outputChannelData[0];

        if (isFirstTrack)
        {
            for (int i = 0; i < numSamples; ++i)
                out[i] = ((inL[i] + (inR ? inR[i] : inL[i])) * 0.5f) * trackGainIn;
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
                out[i] += ((inL[i] + (inR ? inR[i] : inL[i])) * 0.5f) * trackGainIn;
        }
    }
    else
    {
        float* outL = outputChannelData[0];
        float* outR = outputChannelData[1];

        if (isFirstTrack)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                outL[i] = inL[i] * trackGainIn;
                outR[i] = (inR ? inR[i] : inL[i]) * trackGainIn;
            }
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
            {
                outL[i] += inL[i] * trackGainIn;
                outR[i] += (inR ? inR[i] : inL[i]) * trackGainIn;
            }
        }
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