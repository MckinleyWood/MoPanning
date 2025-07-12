#pragma once
#include <JuceHeader.h>

/*  This class handles retrieving the audio data from the input file or 
    device.
*/
class AudioEngine : public juce::AudioSource
{
public:
    //=========================================================================
    AudioEngine();
    ~AudioEngine() override;

    //=========================================================================
    void setAudioCallbackSource(juce::AudioSource* src);
    bool loadFile(const juce::File&);
    void togglePlayback();

    //=========================================================================
    void prepareToPlay(int samplesPerBlock, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;

    //=========================================================================
    bool isPlaying() const;
    void stopPlayback();
    void startPlayback();

private:
    //=========================================================================
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer player;
    
    /*  The transport is the timeline controller that owns the file 
        reader source, supplies buffers to the sound card via 
        AudioSourcePlayer, and keeps track of play-head, looping, etc.
    */
    juce::AudioTransportSource transport;

    std::unique_ptr<juce::AudioFormatReaderSource> fileSource;
    juce::AudioFormatManager formatManager;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};