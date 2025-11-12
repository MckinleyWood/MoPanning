#pragma once
#include <JuceHeader.h>


enum InputType { file, streaming };

//=============================================================================
/*  This class handles retrieving the audio data from the input file or 
    device.
*/
class AudioEngine
{
public:
    //=========================================================================
    AudioEngine();
    ~AudioEngine();

    void fillAudioBuffers(const float *const *inputChannelData,
                          int numInputChannels,
                          float *const *outputChannelData,
                          int numOutputChannels,
                          int numSamples,
                          juce::AudioBuffer<float>& buffer,
                          bool isFirstTrack,
                          float trackGainIn);

    void setInputType(InputType type);

    bool loadFile(const juce::File&);
    void togglePlayback();

    void prepareToPlay(int samplesPerBlock, double sampleRate);
    void releaseResources();

    bool isPlaying() const;
    void stopPlayback();
    void startPlayback();

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

private:
    //=========================================================================
    juce::AudioDeviceManager deviceManager;
    
    /*  The transport is the timeline controller that owns the file 
        reader source, supplies buffers to the sound card via 
        AudioSourcePlayer, and keeps track of play-head, looping, etc.
    */
    juce::AudioTransportSource transport;

    std::unique_ptr<juce::AudioFormatReaderSource> fileSource;
    juce::AudioFormatManager formatManager;

    InputType inputType;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};