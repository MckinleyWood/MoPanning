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
class PageComponent : public juce::Component
{
public:
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::unique_ptr<CustomAudioDeviceSelectorComponent> deviceSelector;

    void resized() override
    {
        jassert (controls.size() == labels.size());

        auto bounds = getLocalBounds();

        // Device selector
        if (deviceSelector != nullptr)
        {
            auto h = deviceSelector->getHeight();
            deviceSelector->setBounds(bounds.removeFromTop(h));
        }

        int j = 0;
        juce::Rectangle<int> vSliderArea;
        juce::Rectangle<int> vSliderLabelArea;

        // Lay out controls
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto* label = labels[i].get();
            auto* ctrl = controls[i].get();

            // Skip items that are hidden
            if (!label->isVisible() || !ctrl->isVisible())
                continue;

            // Combos
            if (auto* control = dynamic_cast<ComboBox*>(ctrl))
            {
                // setBounds will change size, so store width and height
                auto w = ctrl->getWidth();
                auto h = ctrl->getHeight();

                auto zone = bounds.removeFromTop(25);

                auto comboZone = zone.removeFromRight(200);
                auto comboLabelZone = zone.removeFromLeft(150);

                label->setBounds(comboLabelZone);

                ctrl->setBounds(comboZone);
                
                ctrl->setSize(w, h);
            }

            // Sliders
            else if (auto* slider = dynamic_cast<juce::Slider*>(ctrl))
            {
                // Vertical gain sliders
                if (slider->getSliderStyle() == juce::Slider::SliderStyle::LinearVertical)
                {
                    if (j == 0)
                    {
                        vSliderArea = bounds.removeFromTop(150);
                        vSliderLabelArea = bounds.removeFromTop(25);
                        ++j;
                    }

                    int gainSpacing = ctrl->getWidth();

                    label->setBounds(vSliderLabelArea.removeFromLeft(gainSpacing));
                    label->setJustificationType(juce::Justification::centred);

                    ctrl->setBounds(vSliderArea.removeFromLeft(gainSpacing));
                    ctrl->setSize(gainSpacing, 150);
                }

                // Horizontal sliders
                else
                {
                    auto zone = bounds.removeFromTop(25);

                    auto sliderZone = zone.removeFromRight(200);
                    auto sliderLabelZone = zone.removeFromLeft(150);

                    label->setBounds(sliderLabelZone);

                    ctrl->setBounds(sliderZone);
                    ctrl->setSize(200, 25);
                }
            }

            bounds.removeFromTop(12);
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

    void updateParamVisibility(int numTracksIn, bool threeDimIn);

    std::unordered_map<juce::String, juce::Component*> parameterComponentMap;
    std::unordered_map<juce::String, juce::Label*> parameterLabelMap;
    int numTracks = 1;
    int dim = 1;

private:
    //=========================================================================
    MainController& controller;

    // Title label subcomponent
    juce::Label title;

    // Record button
    std::unique_ptr<juce::ToggleButton> recordButton;

    // Label pointers
    std::vector<std::unique_ptr<juce::Label>> labels;

    // Attachment pointers
    std::vector<std::unique_ptr<apvts::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<apvts::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<apvts::ButtonAttachment>> buttonAttachments;

    bool initialized = false;

    std::unique_ptr<juce::TabbedComponent> tabs;
    std::unique_ptr<PageComponent> ioPage;
    std::unique_ptr<PageComponent> visualPage;
    std::unique_ptr<PageComponent> analysisPage;
    std::unique_ptr<PageComponent> colorsPage;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};

//=============================================================================
class SettingsWindow : public juce::DocumentWindow
{
public:
    SettingsWindow(MainController& controller)
        : DocumentWindow("Settings",
                         juce::Colours::darkgrey,
                         DocumentWindow::allButtons,
                         true)   // addToDesktop
    {
        setUsingNativeTitleBar(true);

        // Create the settings component
        settings = std::make_unique<SettingsComponent>(controller);

        setResizable(true, true);
        setContentOwned(settings.get(), true);

        centreWithSize(400, 600);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }

private:
    std::unique_ptr<SettingsComponent> settings;
};