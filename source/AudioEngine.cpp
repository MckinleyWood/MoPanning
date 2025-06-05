#include "AudioEngine.h"

//=============================================================================
AudioEngine::AudioEngine()
{
    // Tell JUCE which formats we can open
    formatManager.registerBasicFormats();

    // Open the default output device (0 inputs, 2 outputs)
    deviceManager.initialise(0, 2, nullptr, true);

    // Route our transport into the device
    player.setSource(&transport);
    deviceManager.addAudioCallback(&player);
}

//=============================================================================
AudioEngine::~AudioEngine()
{
    player.setSource(nullptr);
    deviceManager.removeAudioCallback(&player);
    transport.setSource(nullptr);
}

//=============================================================================
bool AudioEngine::loadFile(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor (file);
    if (reader == nullptr)
        return false; // unsupported / unreadable

    auto newSource = std::make_unique<juce::AudioFormatReaderSource>
                                            (reader, true /*readerOwned*/);

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

//=============================================================================
void AudioEngine::prepareToPlay (int samplesPerBlock, double sampleRate)
{
    transport.prepareToPlay(samplesPerBlock, sampleRate);
}

void AudioEngine::releaseResources()
{
    transport.releaseResources();
}

void AudioEngine::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    transport.getNextAudioBlock(info);
}
