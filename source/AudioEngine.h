/* AudioIOEngine

This file handles retrieving the audio data from the input file or 
device.
*/

#pragma once
#include <JuceHeader.h>

class AudioEngine : public juce::AudioSource
{
public:
    //=========================================================================
    AudioEngine();
    ~AudioEngine() override;

    //=========================================================================
    bool loadFile(const juce::File&);

    //=========================================================================
    void prepareToPlay(int samplesPerBlock, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;

private:
    //=========================================================================
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer player;
    
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> fileSource;
    juce::AudioFormatManager formatManager;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};