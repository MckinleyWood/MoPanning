/* MainController

This file handles the control logic and owns the audio engine, 
transport, and parameters, and supplies data the the other pieces. All
communication between parts of the program must run through here.
*/

#pragma once
#include <JuceHeader.h>
#include "AudioAnalyzer.h"
#include "AudioEngine.h"

//=============================================================================
class MainController : public juce::AudioSource
{
public:
    //=========================================================================
    MainController();
    ~MainController() override;

    //=========================================================================
    bool loadFile(const juce::File& f);
    void togglePlayback();

    //=========================================================================
    void prepareToPlay(int samplesPerBlock, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

private:
    //=========================================================================
    AudioEngine engine;
    AudioAnalyzer analyzer;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainController)
};