#pragma once
#include <JuceHeader.h>

using apvts = juce::AudioProcessorValueTreeState;

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
    : public juce::Component/* ,
      private juce::ComboBox::Listener,
      private juce::Slider::Listener */
{
public:
    //=========================================================================
    SettingsContentComponent(MainController& c);
    ~SettingsContentComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Helpers
    std::vector<juce::Component*> getSettings();
    std::vector<juce::Label*> getLabels();
    int getDeviceSelectorHeight() const;
    std::vector<juce::Component*> getUIObjects() { return uiObjects; }

private:
    //=========================================================================
    MainController& controller;

    // Title label subcomponent
    juce::Label title;

    // Device selector
    std::unique_ptr<CustomAudioDeviceSelectorComponent> deviceSelector;

    // Attachment pointers
    std::vector<std::unique_ptr<apvts::ComboBoxAttachment>> 
        comboAttachments;
    std::vector<std::unique_ptr<apvts::SliderAttachment>> 
        sliderAttachments;

    // UI object pointers
    std::vector<juce::Component*> uiObjects;
    std::vector<juce::Label*> labels;

    bool initialized = false;
};