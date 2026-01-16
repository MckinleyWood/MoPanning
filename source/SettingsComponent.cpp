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
    // Update parameter visibility when numTracks changes
    controller.onNumTracksChanged = [this](int numTracksIn)
    {
        numTracks = numTracksIn;
        updateParamVisibility(numTracks, dim);
    };

    // Update parameter visibility when dim changes
    controller.onDimChanged = [this](int dimIn)
    {
        dim = dimIn;
        updateParamVisibility(numTracks, dim);
    };

    // Update device selector visibility when inputType changes
    controller.onInputTypeChanged = [this] (int inputTypeIn)
    {
        const bool isStreaming = inputTypeIn;

        if (ioPage->deviceSelector != nullptr)
            ioPage->deviceSelector->setDeviceControls(isStreaming);

        resized();
    };


    using Font = juce::FontOptions;

    // Set the fonts
    Font normalFont = {14.f, 0};
    Font titleFont = normalFont.withHeight(30.0f).withStyle("Bold");

    // Set up title label
    title.setText("MoPanning Settings", juce::dontSendNotification);
    title.setFont(titleFont);
    title.setJustificationType(Justification::left);
    title.setColour(Label::textColourId, Colours::linen);
    addAndMakeVisible(title);

    // 
    auto parameters = controller.getParameterDescriptors();
    auto& apvts = controller.getAPVTS();

    // Recording button
    recordButton = std::make_unique<juce::ToggleButton>("Recording");
    recordButton->setButtonText("Record");
    addAndMakeVisible(recordButton.get());

    auto recordAttachment = std::make_unique<apvts::ButtonAttachment>(
        apvts, "recording", *recordButton);
    buttonAttachments.push_back(std::move(recordAttachment));

    // Recording button label
    recordButtonLabel = std::make_unique<juce::Label>();
    recordButtonLabel->setText("Record", juce::dontSendNotification);
    juce::FontOptions recordButtonFont = {14.f, 0};
    recordButtonLabel->setFont(recordButtonFont);
    recordButton->setClickingTogglesState(true);
    addAndMakeVisible(recordButtonLabel.get());

    // Create tabs and pages for setting groups
    tabs = std::make_unique<juce::TabbedComponent>(juce::TabbedButtonBar::TabsAtTop);
    addAndMakeVisible(tabs.get());

    ioPage = std::make_unique<PageComponent>();
    visualPage = std::make_unique<PageComponent>();
    analysisPage = std::make_unique<PageComponent>();
    colorsPage = std::make_unique<PageComponent>();

    Colour backgroundColour = getLookAndFeel().findColour(ResizableWindow::backgroundColourId);

    tabs->addTab("I/O", backgroundColour, ioPage.get(), false);
    tabs->addTab("Visual", backgroundColour, visualPage.get(), false);
    tabs->addTab("Analysis", backgroundColour, analysisPage.get(), false);
    tabs->addTab("Colours", backgroundColour, colorsPage.get(), false);

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
    for (auto& p : parameters)
    {
        // Skip hidden parameters
        if (p.display == false)
        {
            continue;
        }

        if (p.id == "recording")
        {
            continue; // special cases--already handled above
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

        auto label = std::make_unique<juce::Label>();
        label->setText(p.displayName, juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centredLeft);
        label->setFont(normalFont);

        if (p.type == ParameterDescriptor::Float)
        {
            auto slider = std::make_unique<Slider>(p.displayName);
            slider->setRange(p.range.start, p.range.end);
            slider->setValue(p.defaultValue);
            slider->setTextValueSuffix(p.unit);
            slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxLeft, false, 55, 15);
            
            auto attachment = std::make_unique<apvts::SliderAttachment>(
                apvts, p.id, *slider);

            if (p.id.contains("track"))  // vertical sliders for track gains
            {
                slider->setSliderStyle(Slider::SliderStyle::LinearVertical);
                slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxBelow, false, 35, 15);
                slider->setSize(45, 150);
                label->setJustificationType(juce::Justification::centred);
            }
                
            page->addAndMakeVisible(*slider);

            sliderAttachments.push_back(std::move(attachment));
            page->controls.push_back(std::move(slider));
        }
        else if (p.type == ParameterDescriptor::Choice)
        {
            auto combo = std::make_unique<juce::ComboBox>(p.displayName);
            combo->addItemList(p.choices, 1); // JUCE items start at index 1
            combo->setSelectedItemIndex(static_cast<int>(p.defaultValue));
            combo->setJustificationType(juce::Justification::centred);

            if (p.id == "inputType")
                combo->setSize(160, 24);
            else if (p.choices[0].length() < 7 && p.choices[1].length() < 7)
                combo->setSize(80, 24);
            else if (p.choices[0].length() < 10 && p.choices[1].length() < 10)
                combo->setSize(115, 24);
            else
                combo->setSize(150, 24);
            
            auto attachment = std::make_unique<apvts::ComboBoxAttachment>(
                apvts, p.id, *combo);
                
            page->addAndMakeVisible(*combo);
            comboAttachments.push_back(std::move(attachment));

            if (p.id == "inputType")
            {
                page->inputTypeCombo = std::move(combo);
                page->inputTypeLabel = std::move(label);
                page->inputTypeLabel->setJustificationType(juce::Justification::bottomRight);
                page->addAndMakeVisible(*page->inputTypeLabel);
                continue;
            }
            else
            {
                page->controls.push_back(std::move(combo));
            }
        }

        page->addAndMakeVisible(*label);
        page->labels.push_back(std::move(label));

        // For dynamic resizing
        parameterComponentMap[p.id] = page->controls.back().get();
        parameterLabelMap[p.id] = page->labels.back().get();
    }

    initialized = true;
}

//=============================================================================
SettingsComponent::~SettingsComponent()
{
    // Clear attachments before APVTS is deleted
    sliderAttachments.clear();
    comboAttachments.clear();
}

//=============================================================================
void SettingsComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Lay out the title at the top
    auto titleZone = bounds.removeFromTop(30);
    title.setBounds(titleZone);

    // Lay out record button next to title
    auto recordButtonZone = titleZone.removeFromRight(80);
    recordButton->setBounds(recordButtonZone.getCentreX() + 10, recordButtonZone.getCentreY() - 9, 20, 20);
    recordButtonLabel->setBounds(recordButtonZone.getCentreX() - 38, recordButtonZone.getCentreY() - 9, 50, 20);

    // Tabs below title
    tabs->setBounds(bounds.withTrimmedTop(10));
}

//=============================================================================
void SettingsComponent::updateParamVisibility(int numTracksIn, bool showGridSettingIn)
{
    const bool showGridSetting = (showGridSettingIn == 0);

    auto setVisibleIfFound = [this](const juce::String& id, bool visible)
    {
        if (auto* comp = parameterComponentMap[id])
            comp->setVisible(visible);
        if (auto* label = parameterLabelMap[id])
            label->setVisible(visible);
    };

    for (int track = 2; track <= 8; ++track)
    {
        bool shouldShow = (numTracksIn >= track);

        juce::String colourID = "track" + juce::String(track) + "ColourScheme";
        juce::String gainID   = "track" + juce::String(track) + "Gain";

        setVisibleIfFound(colourID, shouldShow);
        setVisibleIfFound(gainID, shouldShow);
    }

    setVisibleIfFound("showGrid", showGridSetting);
        
    if (ioPage)       ioPage->resized();
    if (visualPage)   visualPage->resized();
    if (analysisPage) analysisPage->resized();
    if (colorsPage)   colorsPage->resized();

    repaint();
}
