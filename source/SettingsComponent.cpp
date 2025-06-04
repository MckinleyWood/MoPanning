#include "SettingsComponent.h"
#include "MainController.h"

//=============================================================================
SettingsComponent::SettingsComponent(MainController& c) : controller(c)
{
    // Add buttons, etc. here later
}

SettingsComponent::~SettingsComponent() = default;

//=============================================================================
void SettingsComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawFittedText("Hi Owen",
                     getLocalBounds(),
                     juce::Justification::centred,
                     1);
}

void SettingsComponent::resized() 
{
    // Layout child widgets here
}
