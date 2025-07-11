#pragma once
#include <JuceHeader.h>

class MainController;

//=============================================================================
/*  This is the component for the settings widget.
*/
class SettingsComponent : public juce::Component,
                          private juce::ComboBox::Listener,
                          private juce::Slider::Listener
{
public:
    //=========================================================================
    explicit SettingsComponent(MainController&);
    ~SettingsComponent() override;

    //=========================================================================
    void paint(juce::Graphics&) override;
    void resized(void) override;

    void comboBoxChanged(juce::ComboBox* s) override;
    void sliderValueChanged(juce::Slider* s) override;

private:
    //=========================================================================
    MainController& controller;

    // Title label subcomponent
    juce::Label title;

    // Settings subcomponents
    juce::ComboBox sampleRateBox;
    juce::ComboBox samplesPerBlockBox;
    juce::ComboBox analysisModeBox;
    juce::ComboBox fftOrderBox;
    juce::ComboBox minFrequencyBox;
    juce::ComboBox numCQTbinsBox;
    juce::Slider recedeSpeedSlider;
    juce::Slider dotSizeSlider;
    juce::Slider nearZSlider;
    juce::Slider fadeEndZSlider;
    juce::Slider farZSlider;
    juce::Slider fovSlider;

    // Labels
    juce::Label sampleRateLabel;
    juce::Label samplesPerBlockLabel;
    juce::Label analysisModeLabel;
    juce::Label fftOrderLabel;
    juce::Label minFrequencyLabel;
    juce::Label numCQTbinsLabel;
    juce::Label recedeSpeedLabel;
    juce::Label dotSizeLabel;
    juce::Label nearZLabel;
    juce::Label fadeEndZLabel;
    juce::Label farZLabel;
    juce::Label fovLabel;

    // Functions to get a list of all of our settings subcomponents and labels
    std::vector<juce::Component*> getSettings();
    std::vector<juce::Label*> getLabels();

    bool initialized = false;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};