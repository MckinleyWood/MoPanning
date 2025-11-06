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
        DBG("OldHeight = " << oldHeight << ", NewHeight: " << newHeight);
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

    int oldDeviceSelectorHeight = 0;

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
    void updateParamVisibility(int numTracksIn, bool threeDimIn);

    // Helpers
    int getDeviceSelectorHeight() const;
    const std::vector<std::unique_ptr<juce::Component>>& getUIObjects() const;

    std::unordered_map<juce::String, juce::Component*> parameterComponentMap;
    std::unordered_map<juce::String, juce::Label*> parameterLabelMap;
    int numTracks = 1;
    int dim = 1;

private:
    //=========================================================================
    MainController& controller;

    // Title label subcomponent
    juce::Label title;

    // Device selector
    std::unique_ptr<CustomAudioDeviceSelectorComponent> deviceSelector;

    // Attachment pointers
    std::vector<std::unique_ptr<apvts::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<apvts::SliderAttachment>> sliderAttachments;

    // UI object pointers
    std::vector<std::unique_ptr<juce::Component>> uiObjects;
    std::vector<std::unique_ptr<juce::Label>> labels;

    bool initialized = false;
};