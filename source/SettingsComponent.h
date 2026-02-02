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

// Helper for finding child components of device selector
static juce::Component* findAudioPanelRecursively (juce::Component* parent)
{
    if (parent == nullptr)
        return nullptr;

    // Case A: Is this a TabbedComponent?
    if (auto* tabs = dynamic_cast<juce::TabbedComponent*>(parent))
    {
        if (auto* content = tabs->getTabContentComponent(0))
            return content;

        return nullptr;
    }

    // Case B: Panel is the *direct* child of AudioDeviceSelectorComponent
    // (internal AudioDeviceSettingsPanel)
    if (parent->getNumChildComponents() == 1)
    {
        return parent->getChildComponent(0);
    }

    // Fallback: recursively search children
    for (int i = 0; i < parent->getNumChildComponents(); ++i)
        if (auto* found = findAudioPanelRecursively(parent->getChildComponent(i)))
            return found;

    return nullptr;
}

//=============================================================================
class CustomAudioDeviceSelectorComponent 
    : public juce::AudioDeviceSelectorComponent
{
public:
    std::function<void()> onHeightChanged;
    std::function<void()> onInputTypeChanged;

    using juce::AudioDeviceSelectorComponent::AudioDeviceSelectorComponent;

    void resized() override
    {
        if (inResized)
            return;

        const juce::ScopedValueSetter<bool> guard(inResized, true);

        auto oldHeight = getHeight();
        juce::AudioDeviceSelectorComponent::resized();
        auto newHeight = getHeight();

        if (oldHeight != newHeight && onHeightChanged)
            onHeightChanged();
    }

    void setDeviceControls(bool streaming)
    {
        if (auto* panel = findAudioPanelRecursively(this))
        {
            // Output elements
            const int outputButtonIndex       = 0;
            const int outputDropdownIndex     = 1;
            const int outputLabelIndex        = 2;
            const int outputListBoxIndex      = 7;
            const int outputListBoxLabelIndex = 8;

            // Input elements
            const int inputButtonIndex        = 3;
            const int inputDropdownIndex      = 4;
            const int inputLabelIndex         = 5;
            const int inputMeterIndex         = 6;
            const int inputListBoxIndex       = 13;

            auto trySetVisible = [panel](int index, bool vis)
            {
                if (index < panel->getNumChildComponents())
                    panel->getChildComponent(index)->setVisible(vis);
            };

            // Get the input dropdown
            if (auto* inputBox = dynamic_cast<juce::ComboBox*>(panel->getChildComponent(inputDropdownIndex)))
            {
                // Center justification
                inputBox->setJustificationType(juce::Justification::centred);

                if (streaming == false)
                {
                    // Currently selected input for later
                    selected = inputBox->getSelectedId();
                    
                    // Set input to << none >>
                    inputBox->setSelectedId(-1);
                    
                    // Lock the box so the user cannot change it
                    inputBox->setEnabled(false);
                }
                else 
                {
                    inputBox->setEnabled(true);

                    inputBox->setText(selectedText, juce::dontSendNotification);
                    int numItems = inputBox->getNumItems();
                    if (numItems > 0)
                        inputBox->setSelectedId(selected);
                }
            }

            if (auto* outputBox = dynamic_cast<juce::ComboBox*>(panel->getChildComponent(outputDropdownIndex)))
            {
                // Center justification
                outputBox->setJustificationType(juce::Justification::centred);

                int x = outputBox->getX();
                int y = outputBox->getY();
                int w = outputBox->getWidth();
                int h = outputBox->getHeight();

                outputBox->setBounds(x - 50 , y, w, h);
            }

            panel->resized();
        }

        resized();
        if (onHeightChanged) onHeightChanged();
    }

private:
    juce::PropertyPanel* findPropertyPanel()
    {
        for (auto* child : getChildren())
            if (auto* panel = dynamic_cast<juce::PropertyPanel*>(child))
                return panel;

        return nullptr;
    }

    int selected = 0;
    String selectedText;
    bool inResized = false;
};

//=============================================================================
class PageComponent : public juce::Component
{
public:
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::unique_ptr<CustomAudioDeviceSelectorComponent> deviceSelector;
    std::unique_ptr<ComboBox> inputTypeCombo;
    std::unique_ptr<juce::Label> inputTypeLabel;
    juce::Label fadersLabel;

    bool firstGainSlider = true;

    void resized() override
    {
        jassert (controls.size() == labels.size());

        firstGainSlider = true;
        auto bounds = getLocalBounds().withTrimmedTop(10);

        // Input type
        if (inputTypeCombo != nullptr && inputTypeLabel != nullptr)
        {
            auto w = inputTypeCombo->getWidth();
            auto h = inputTypeCombo->getHeight();

            auto bounds2 = bounds;
            auto zone = bounds.removeFromTop(15);
            auto zone2 = bounds2.removeFromTop(25);
            zone2.removeFromRight(235);
            auto comboZone = zone.removeFromRight(234);
            auto comboLabelZone = zone2.removeFromRight(100);
            comboLabelZone.removeFromBottom(5);

            inputTypeCombo->setBounds(comboZone);
            inputTypeLabel->setBounds(comboLabelZone);
            inputTypeCombo->setSize(w, h);
        }

        // Device selector and input type
        if (deviceSelector != nullptr)
        {
            auto h = deviceSelector->getHeight();
            auto deviceSelectorBounds = bounds;
            deviceSelector->setBounds(deviceSelectorBounds.removeFromTop(h));
            bounds.removeFromTop(h - 20);
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

                // const int comboCenterX = getWidth() - 100; // global anchor
                // const auto comboZone =
                //     ctrl->getBounds().withCentre(
                //         { comboCenterX, zone.getCentreY() }
                //     );

                label->setBounds(comboLabelZone);
                ctrl->setBounds(comboZone);
                ctrl->setSize(w, h);
            }

            // Sliders
            else if (auto* slider = dynamic_cast<Slider*>(ctrl))
            {
                // Vertical gain sliders
                if (slider->getSliderStyle() == Slider::SliderStyle::LinearVertical)
                {
                    if (firstGainSlider == true)
                    {
                        fadersLabel.setText("Faders", juce::dontSendNotification);
                        juce::FontOptions fadersFont = {20.f, 0};
                        fadersLabel.setFont(fadersFont);
                        fadersLabel.setJustificationType(Justification::centred);
                        fadersLabel.setColour(Label::textColourId, Colours::linen);
                        addAndMakeVisible(fadersLabel);
                        fadersLabel.setBounds(bounds.removeFromTop(20));
    
                        firstGainSlider = false;
                    }

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

            // Spacing
            bounds.removeFromTop(20);
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

    void updateParamVisibility(int numTracksIn, bool showGridSettingIn);

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
    std::unique_ptr<juce::Label> recordButtonLabel;

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
        : DocumentWindow("",
                         juce::Colour::fromRGB(30, 30, 30),
                         DocumentWindow::allButtons,
                         true)   // addToDesktop
    {
        setUsingNativeTitleBar(true);

        // Create the settings component
        settings = std::make_unique<SettingsComponent>(controller);

        setResizable(false, false);
        setContentOwned(settings.get(), true);

        centreWithSize(400, 700);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }

private:
    std::unique_ptr<SettingsComponent> settings;
};