#pragma once
#include <JuceHeader.h>

class MainController;

//=============================================================================
class CustomAudioDeviceSelectorComponent 
    : public juce::AudioDeviceSelectorComponent
{
public:
    std::function<void()> onHeightChanged;

    using juce::AudioDeviceSelectorComponent::AudioDeviceSelectorComponent;

    void resized() override
    {
        auto oldHeight = getHeight();
        juce::AudioDeviceSelectorComponent::resized();
        auto newHeight = getHeight();

        if (oldHeight != newHeight && onHeightChanged)
            onHeightChanged();
    }
};

//=============================================================================
class NonScrollingSlider : public juce::Slider
{
public:
    using juce::Slider::Slider;

    void mouseWheelMove(const juce::MouseEvent& event, 
                        const juce::MouseWheelDetails& details) override
    {
        // Scroll the viewport instead
        if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
        {
            MouseEvent e2 = event.getEventRelativeTo(viewport);
            viewport->mouseWheelMove(e2, details);
        }
    }
};

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
class SettingsComponent::SettingsContentComponent 
    : public juce::Component,
      private juce::ComboBox::Listener,
      private juce::Slider::Listener
{
public:
    //=========================================================================
    SettingsContentComponent(MainController& c);
    ~SettingsContentComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void comboBoxChanged(juce::ComboBox* b) override;
    void sliderValueChanged(juce::Slider* s) override;

    // Helpers
    std::vector<juce::Component*> getSettings();
    std::vector<juce::Label*> getLabels();
    int getDeviceSelectorHeight() const;

private:
    //=========================================================================
    MainController& controller;

    // Title label subcomponent
    juce::Label title;

    // Device selector
    std::unique_ptr<CustomAudioDeviceSelectorComponent> deviceSelector;

    // Settings subcomponents
    juce::ComboBox samplesPerBlockBox;
    juce::ComboBox inputTypeBox;
    juce::ComboBox transformBox;
    juce::ComboBox panMethodBox;
    juce::ComboBox fftOrderBox;
    juce::ComboBox numCQTbinsBox;
    juce::ComboBox minFrequencyBox;
    NonScrollingSlider maxAmplitudeSlider;
    juce::ComboBox dimensionBox;
    juce::ComboBox colourSchemeBox;
    NonScrollingSlider recedeSpeedSlider;
    NonScrollingSlider dotSizeSlider;
    NonScrollingSlider ampScaleSlider;
    NonScrollingSlider nearZSlider;
    NonScrollingSlider fadeEndZSlider;
    NonScrollingSlider farZSlider;
    NonScrollingSlider fovSlider;

    // Labels
    juce::Label samplesPerBlockLabel;
    juce::Label inputTypeLabel;
    juce::Label transformLabel;
    juce::Label panMethodLabel;
    juce::Label fftOrderLabel;
    juce::Label numCQTbinsLabel;
    juce::Label maxAmplitudeLabel;
    juce::Label minFrequencyLabel;
    juce::Label colourSchemeLabel;
    juce::Label dimensionLabel;
    juce::Label recedeSpeedLabel;
    juce::Label dotSizeLabel;
    juce::Label ampScaleLabel;
    juce::Label nearZLabel;
    juce::Label fadeEndZLabel;
    juce::Label farZLabel;
    juce::Label fovLabel;

    bool initialized = false;
};