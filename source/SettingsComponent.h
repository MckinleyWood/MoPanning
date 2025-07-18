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
    // void paint(juce::Graphics&) override;
    void resized(void) override;

private:
    //=========================================================================
    MainController& controller;

    // Viewport and content
    juce::Viewport viewport;

    // Declare inner component
    class SettingsContentComponent;
    std::unique_ptr<SettingsContentComponent> content;

    bool initialized = false;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};

//=============================================================================
class SettingsComponent::SettingsContentComponent : public juce::Component,
                                                    private juce::ComboBox::Listener,
                                                    private juce::Slider::Listener
{
public:
    //=========================================================================
    SettingsContentComponent(MainController& c);
    ~SettingsContentComponent();

    void resized() override;
    void paint(juce::Graphics& g) override;

    void comboBoxChanged(juce::ComboBox* b) override;
    void sliderValueChanged(juce::Slider* s) override;

    // Helpers
    std::vector<juce::Component*> getSettings();
    std::vector<juce::Label*> getLabels();

private:
    //=========================================================================
    MainController& controller;

    // Title label subcomponent
    juce::Label title;

    // Settings subcomponents
    juce::ComboBox sampleRateBox;
    juce::ComboBox samplesPerBlockBox;
    juce::ComboBox transformBox;
    juce::ComboBox panMethodBox;
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
    juce::Label transformLabel;
    juce::Label panMethodLabel;
    juce::Label fftOrderLabel;
    juce::Label minFrequencyLabel;
    juce::Label numCQTbinsLabel;
    juce::Label recedeSpeedLabel;
    juce::Label dotSizeLabel;
    juce::Label nearZLabel;
    juce::Label fadeEndZLabel;
    juce::Label farZLabel;
    juce::Label fovLabel;

    bool initialized = false;
};