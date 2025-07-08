#include "SettingsComponent.h"
#include "MainController.h"

//=============================================================================
SettingsComponent::SettingsComponent(MainController& c) : controller(c)
{
    addAndMakeVisible(title);
    for (const auto& setting : getSettings())
    {
        addAndMakeVisible(setting);
    }
}

SettingsComponent::~SettingsComponent()
{
}

//=============================================================================
void SettingsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(10, 10);

    // Paint the background
    g.fillAll(juce::Colours::darkgrey);

    using Font = juce::FontOptions;

    // Set the font
    Font normalFont = { "Comic Sans MS", 20.f, 0};
    Font titleFont = normalFont.withHeight(bounds.getHeight() * 0.06f)
                               .withStyle("Bold");

    // Paint the title
    title.setText("Hi Owen", juce::dontSendNotification);
    title.setFont(titleFont);
    title.setJustificationType(juce::Justification::centred);
    title.setColour(juce::Label::textColourId, juce::Colours::white);

    // Paint boxes around all components (for testing)
    // g.setColour(juce::Colours::yellow);
    // g.drawRect(title.getBounds());
    // for (const auto& comp : getSettings())
    // {
    //     g.drawRect(comp->getBounds());
    // }
}

void SettingsComponent::resized() 
{
    // Partition the editor into zones
    auto bounds = getLocalBounds();

    bounds.reduce(10, 10);
    int totalHeight = bounds.getHeight();
    int totalwidth = bounds.getWidth();

    auto titleZone = bounds.withTrimmedBottom(totalHeight * 0.9f + 10);
    auto mainZone = bounds.withTrimmedTop(totalHeight * 0.1f);

    std::vector<juce::Rectangle<int>> settingZones;

    auto settings = getSettings();
    int numSettings = settings.size();
    int rowHeight = mainZone.getHeight() * 1.f / numSettings;

    juce::Rectangle<int> currentZone;
    
    for (int i = 0; i < numSettings; ++i)
    {
        currentZone = mainZone.removeFromTop(rowHeight);
        currentZone.reduce(0, 5);
        settingZones.push_back(currentZone);
    }

    // Set the bounds of the components
    title.setBounds(titleZone);

    for(int i = 0; i < numSettings; ++i)
    {
        settings[i]->setBounds(settingZones[i]);
    }
}

void SettingsComponent::sliderValueChanged(juce::Slider* s)
{

}

//=============================================================================
std::vector<juce::Component*> SettingsComponent::getSettings()
{
    return {
        &sampleRateBox,
        &samplesPerBlockBox,
        &analysisModeBox,
        &fftOrderBox,
        &minFrequencyBox,
        &numCQTbinsBox,
        &recedeSpeedSlider,
        &dotSizeSlider,
        &nearZSlider,
        &fadeEndZSlider,
        &farZSlider,
        &fovSlider
    };
}
