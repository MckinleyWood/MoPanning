#include "AudioEngine.h"

//=============================================================================
AudioEngine::AudioEngine()
{
    // Tell JUCE which formats we can open
    formatManager.registerBasicFormats();

    // Open the default output device (0 inputs, 2 outputs)
    deviceManager.initialise(0, 2, nullptr, true);
}

//=============================================================================
AudioEngine::~AudioEngine()
{
    player.setSource(nullptr);
    deviceManager.removeAudioCallback(&player);
    transport.setSource(nullptr);
}

//=============================================================================
void AudioEngine::setAudioCallbackSource(juce::AudioSource* src)
{
    // Disconnect previous
    deviceManager.removeAudioCallback(&player);
    player.setSource(nullptr);
    
    // Connect new
    player.setSource(src);
    deviceManager.addAudioCallback(&player);
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

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    transport.getNextAudioBlock(info);
}
