/*=============================================================================

    This file is part of the MoPanning audio visuaization tool.
    Copyright (C) 2025 Owen Ohlson and Mckinley Wood

    This program is free software: you can redistribute it and/or modify 
    it under the terms of the GNU Affero General Public License as 
    published by the Free Software Foundation, either version 3 of the 
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but 
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
    Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public 
    License along with this program. If not, see 
    <https://www.gnu.org/licenses/>.

=============================================================================*/

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