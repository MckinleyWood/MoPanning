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

#include "SettingsComponent.h"
#include "MainController.h"

//=============================================================================
SettingsComponent::SettingsComponent(MainController& c) : controller(c)
{
    content = std::make_unique<SettingsContentComponent>(controller);
    viewport.setViewedComponent(content.get(), false);
    addAndMakeVisible(viewport);

    controller.onNumTracksChanged = [this](int numTracksIn)
    {
        if (content)
        {
            content->numTracks = numTracksIn;
            content->updateParamVisibility(content->numTracks,
                                        content->dim);
        }
    };

    controller.onDimChanged = [this](int dimIn)
    {
        if (content)
        {
            content->dim = dimIn;
            content->updateParamVisibility(content->numTracks,
                                        content->dim);
        }
    };
}

//=============================================================================
SettingsComponent::~SettingsComponent() = default;

//=============================================================================
void SettingsComponent::resized() 
{
    viewport.setBounds(getLocalBounds());
    const int deviceSelectorHeight = content->getDeviceSelectorHeight();
    int selectorDiff = deviceSelectorHeight - oldDeviceSelectorHeight;
    int contentHeight = 0;
    if (selectorDiff > 1e-6)
        contentHeight = content ? content->getHeight() + selectorDiff : getHeight();

    if (selectorDiff < -1e-6)
        contentHeight = content ? content->getHeight() - selectorDiff : getHeight();

    oldDeviceSelectorHeight = deviceSelectorHeight;
    int contentWidth = getWidth() - 8; // leave some space for scroll bar

    if (content != nullptr)
    {
        content->setSize(contentWidth, contentHeight);
    }
}

//=============================================================================
using sc = SettingsComponent;

sc::SettingsContentComponent::SettingsContentComponent(MainController& c) 
                                                       : controller(c)
{
    using Font = juce::FontOptions;

    // Set the font
    Font normalFont = {13.f, 0};
    Font titleFont = normalFont.withHeight(40).withStyle("Bold");

    // Set up title label
    title.setText("MoPanning", juce::dontSendNotification);
    title.setFont(titleFont);
    title.setJustificationType(juce::Justification::centred);
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    // Set up device selector--max 16 input channels, 2 output channels
    deviceSelector = std::make_unique<CustomAudioDeviceSelectorComponent>(
        controller.getDeviceManager(),
        2, 16, 2, 2,
        false, false, true, true);

    deviceSelector.get()->onHeightChanged = [this]()
    {
        if (auto* parent = findParentComponentOfClass<SettingsComponent>())
        {
            parent->resized();
        }
    };

    addAndMakeVisible(deviceSelector.get());

    // Set up parameter controls
    auto parameters = controller.getParameterDescriptors();
    auto& apvts = controller.getAPVTS();
    for (const auto& p : parameters)
    {
        if (p.display == false)
        {
            continue;
        }
        if (p.type == ParameterDescriptor::Float)
        {
            auto slider = std::make_unique<NonScrollingSlider>(p.displayName);
            slider->setRange(p.range.start, p.range.end);
            slider->setValue(p.defaultValue);
            slider->setTextValueSuffix(p.unit);
            addAndMakeVisible(*slider);

            auto attachment = std::make_unique<apvts::SliderAttachment>(
                apvts, p.id, *slider);
            sliderAttachments.push_back(std::move(attachment));

            uiObjects.push_back(std::move(slider));
        }
        else if (p.type == ParameterDescriptor::Choice)
        {
            auto combo = std::make_unique<juce::ComboBox>(p.displayName);
            combo->addItemList(p.choices, 1); // JUCE items start at index 1
            combo->setSelectedItemIndex(static_cast<int>(p.defaultValue));
            addAndMakeVisible(*combo);

            auto attachment = std::make_unique<apvts::ComboBoxAttachment>(
                apvts, p.id, *combo);
            comboAttachments.push_back(std::move(attachment));

            uiObjects.push_back(std::move(combo));
        }

        auto label = std::make_unique<juce::Label>();
        label->setText(p.displayName, juce::dontSendNotification);
        label->setJustificationType(juce::Justification::left);
        label->setFont(normalFont);
        addAndMakeVisible(*label);
        labels.push_back(std::move(label));

        parameterComponentMap[p.id] = uiObjects.back().get();
        parameterLabelMap[p.id] = labels.back().get();
    }

    initialized = true;
}

sc::SettingsContentComponent::~SettingsContentComponent()
{
    // Clear attachments before APVTS is deleted
    sliderAttachments.clear();
    comboAttachments.clear();
}

void sc::SettingsContentComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    const int rowHeight = 50;
    const int labelHeight = 14;
    int numSettings = (int)uiObjects.size();

    // Lay out the title at the top
    auto titleZone = bounds.removeFromTop(60);
    title.setBounds(titleZone);

    // Lay out the device selector below the title
    auto deviceSelectorHeight = getDeviceSelectorHeight();
    auto deviceSelectorZone = bounds.removeFromTop(
         deviceSelectorHeight);
    deviceSelector->setBounds(deviceSelectorZone);

    // Dynamic layout of visible parameter controls
    int yOffset = bounds.getY() - 30;

    // Lay out the parameter controls in rows below the device selector
    for (int i = 0; i < numSettings; ++i)
    {
        auto* control = uiObjects[i].get();
        auto* label = labels[i].get();

        if (control->isVisible() && label->isVisible())
        {
            auto labelZone = juce::Rectangle<int>(
                bounds.getX(), yOffset, bounds.getWidth(), labelHeight);
            label->setBounds(labelZone);

            yOffset += labelHeight;

            auto controlZone = juce::Rectangle<int>(
                bounds.getX(), yOffset, bounds.getWidth(), rowHeight - labelHeight);
            control->setBounds(controlZone.reduced(0, 4));

            yOffset += (rowHeight - labelHeight);
        }
    }

    // Update total component height to fit all visible elements
    int totalHeight = yOffset + 20;
    setSize(getWidth(), totalHeight);

    // Notify parent (viewport) to update scroll area
    if (auto* parent = findParentComponentOfClass<SettingsContentComponent>())
        parent->resized();
}

void sc::SettingsContentComponent::paint(juce::Graphics& g)
{
    // Paint the background
    g.fillAll(juce::Colours::darkgrey);
}

void sc::SettingsContentComponent::updateParamVisibility(int numTracksIn, bool threeDimIn)
{
    auto show2 = (numTracksIn >= 2);
    auto show3 = (numTracksIn >= 3);
    auto show4 = (numTracksIn >= 4);
    auto show5 = (numTracksIn >= 5);
    auto show6 = (numTracksIn >= 6);
    auto show7 = (numTracksIn >= 7);
    auto show8 = (numTracksIn >= 8);

    bool showGrid = (threeDimIn == 0);

    auto setVisibleIfFound = [this](const juce::String& id, bool visible)
    {
        if (auto* comp = parameterComponentMap[id])
            comp->setVisible(visible);
        if (auto* label = parameterLabelMap[id])
            label->setVisible(visible);
    };

    setVisibleIfFound("track2ColourScheme", show2);
    setVisibleIfFound("track3ColourScheme", show3);
    setVisibleIfFound("track4ColourScheme", show4);
    setVisibleIfFound("track5ColourScheme", show5);
    setVisibleIfFound("track6ColourScheme", show6);
    setVisibleIfFound("track7ColourScheme", show7);
    setVisibleIfFound("track8ColourScheme", show8);

    setVisibleIfFound("track2Gain", show2);
    setVisibleIfFound("track3Gain", show3);
    setVisibleIfFound("track4Gain", show4);
    setVisibleIfFound("track5Gain", show5);
    setVisibleIfFound("track6Gain", show6);
    setVisibleIfFound("track7Gain", show7);
    setVisibleIfFound("track8Gain", show8);


    setVisibleIfFound("showGrid", showGrid);

    resized(); // reposition remaining visible controls
    if (auto* parent = findParentComponentOfClass<SettingsComponent>())
        parent->resized(); // update viewport size
    repaint();
}

//=============================================================================
int sc::SettingsContentComponent::getDeviceSelectorHeight() const
{
    return deviceSelector ? deviceSelector->getHeight() : 0;
}

const std::vector<std::unique_ptr<juce::Component>>& sc::SettingsContentComponent::getUIObjects() const
{
    return uiObjects;
}
