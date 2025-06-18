#pragma once
#include <JuceHeader.h>

class MainController;

//=============================================================================
/*  This is the component for the settings widget.
*/
class SettingsComponent : public juce::Component
{
public:
    //=========================================================================
    explicit SettingsComponent(MainController&);
    ~SettingsComponent() override;

    //=========================================================================
    void paint(juce::Graphics&) override;
    void resized(void) override;

private:
    //=========================================================================
    MainController& controller;

    juce::TextEditor setSampleRate;
    juce::Label sampleRateLabel{"Sample Rate", "Sample Rate [Hz]:"};
    juce::TextEditor setBufferSize;
    juce::Label bufferSizeLabel{"Buffer Size", "Buffer Size:"};
    juce::TextEditor setMinCQTfreq;
    juce::Label minCQTfreqLabel{"Min CQT Freq", "Min CQT Frequency [Hz]:"};
    juce::TextEditor setNumCQTbins;
    juce::Label numCQTbinsLabel{"Num CQT Bins", "Number of CQT Bins:"};
    juce::Slider fftOrderSlider;
    juce::Label fftOrderLabel{"FFT Order", "FFT Order:"};
    
    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};