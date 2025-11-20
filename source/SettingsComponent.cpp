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
    // 
    controller.onNumTracksChanged = [this](int numTracksIn)
    {
        numTracks = numTracksIn;
        updateParamVisibility(numTracks, dim);
    };

    // 
    controller.onDimChanged = [this](int dimIn)
    {
        dim = dimIn;
        updateParamVisibility(numTracks, dim);
    };

    using Font = juce::FontOptions;

    // Set the font
    Font normalFont = {13.f, 0};
    Font titleFont = normalFont.withHeight(40).withStyle("Bold");

    // Set up title label
    title.setText("MoPanning", juce::dontSendNotification);
    title.setFont(titleFont);
    title.setJustificationType(juce::Justification::left);
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    // Recording button
    recordButton = std::make_unique<juce::ToggleButton>("Record");
    recordButton->setButtonText("Record");
    addAndMakeVisible(recordButton.get());
    recordButton->setToggleState(false, juce::dontSendNotification);

    // Create pages for setting groups
    tabs = std::make_unique<juce::TabbedComponent>(juce::TabbedButtonBar::TabsAtTop);
    addAndMakeVisible(tabs.get());

    ioPage = std::make_unique<PageComponent>();
    visualPage = std::make_unique<PageComponent>();
    analysisPage = std::make_unique<PageComponent>();
    colorsPage = std::make_unique<PageComponent>();

    ioPage->isIOPage = true;

    tabs->addTab("I/O", juce::Colours::darkgrey, ioPage.get(), false);
    tabs->addTab("Visual", juce::Colours::darkgrey, visualPage.get(), false);
    tabs->addTab("Analysis", juce::Colours::darkgrey, analysisPage.get(), false);
    tabs->addTab("Colors", juce::Colours::darkgrey, colorsPage.get(), false);

    tabs->setTabBarDepth(30);            // height of tab bar
    tabs->setOutline(0);                 // removes border
    tabs->setIndent(10);                 // optional padding

    // Set up device selector--max 16 input channels, 2 output channels
    ioPage->deviceSelector = std::make_unique<CustomAudioDeviceSelectorComponent>(
        controller.getDeviceManager(),
        2, 16, 2, 2,
        false, false, true, true);

    ioPage->deviceSelector.get()->onHeightChanged = [this]()
    {
        if (ioPage)
            ioPage->resized();   // re-layout items inside page
        
        this->resized();         // re-layout content (title, tabs, page)

    };

    ioPage->addAndMakeVisible(ioPage->deviceSelector.get());

    // Set up parameter controls
    auto parameters = controller.getParameterDescriptors();
    auto& apvts = controller.getAPVTS();
    for (const auto& p : parameters)
    {
        if (p.display == false)
        {
            continue;
        }

        if (p.id == "recording")
        {
            continue; // special case
        }

        PageComponent* page = nullptr;

        if (p.group == "io")
            page = ioPage.get();
        else if (p.group == "visual")
            page = visualPage.get();
        else if (p.group == "analysis")
            page = analysisPage.get();
        else if (p.group == "colors")
            page = colorsPage.get();

        if (p.type == ParameterDescriptor::Float)
        {
            auto slider = std::make_unique<Slider>(p.displayName);
            slider->setRange(p.range.start, p.range.end);
            slider->setValue(p.defaultValue);
            slider->setTextValueSuffix(p.unit);
            
            auto attachment = std::make_unique<apvts::SliderAttachment>(
                apvts, p.id, *slider);

            if (p.id.contains("track"))  // vertical sliders for track gains
            {
                slider->setSliderStyle(Slider::SliderStyle::LinearVertical);
                slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxBelow, false, 40, 20);
            }
                
            page->addAndMakeVisible(*slider);

            page->controls.push_back(std::move(slider));
        }
        else if (p.type == ParameterDescriptor::Choice)
        {
            auto combo = std::make_unique<juce::ComboBox>(p.displayName);
            combo->addItemList(p.choices, 1); // JUCE items start at index 1
            combo->setSelectedItemIndex(static_cast<int>(p.defaultValue));
            page->addAndMakeVisible(*combo);

            auto attachment = std::make_unique<apvts::ComboBoxAttachment>(
                apvts, p.id, *combo);

            page->controls.push_back(std::move(combo));
        }

        auto label = std::make_unique<juce::Label>();
        label->setText(p.displayName, juce::dontSendNotification);
        label->setJustificationType(juce::Justification::left);
        label->setFont(normalFont);
        page->addAndMakeVisible(*label);
        page->labels.push_back(std::move(label));

        parameterComponentMap[p.id] = page->controls.back().get();
        parameterLabelMap[p.id] = page->labels.back().get();
    }

    initialized = true;
}

//=============================================================================
SettingsComponent::~SettingsComponent() = default;

//=============================================================================
void SettingsComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Lay out the title at the top
    auto titleZone = bounds.removeFromTop(60);
    title.setBounds(titleZone);

    // Lay out record button next to title
    auto recordButtonZone = titleZone.removeFromRight(100);
    recordButton->setBounds(recordButtonZone);

    // Tabs below title
    tabs->setBounds(bounds);
}

//=============================================================================
void SettingsComponent::updateParamVisibility(int numTracksIn, bool threeDimIn)
{
    const bool showGrid = (threeDimIn == 0);

    auto setVisibleIfFound = [this](const juce::String& id, bool visible)
    {
        if (auto* comp = parameterComponentMap[id])
            comp->setVisible(visible);
        if (auto* label = parameterLabelMap[id])
            label->setVisible(visible);
    };

    for (int track = 2; track <=8; ++track)
    {
        bool shouldShow = (numTracksIn >= track);

        juce::String colourID = "track" + juce::String(track) + "ColourScheme";
        juce::String gainID   = "track" + juce::String(track) + "Gain";

        setVisibleIfFound(colourID, shouldShow);
        setVisibleIfFound(gainID, shouldShow);
    }

    setVisibleIfFound("showGrid", showGrid);

    if (ioPage)       ioPage->resized();
    if (visualPage)   visualPage->resized();
    if (analysisPage) analysisPage->resized();
    if (colorsPage) colorsPage->resized();

    repaint();
}
