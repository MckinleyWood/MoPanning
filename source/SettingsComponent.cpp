#include "SettingsComponent.h"
#include "MainController.h"

//=============================================================================
SettingsComponent::SettingsComponent(MainController& c) : controller(c)
{
    setSampleRate.setText("44100");
    double sampleRate = setSampleRate.getText().getDoubleValue();
    setSampleRate.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(setSampleRate);
    sampleRateLabel.attachToComponent(&setSampleRate, true);

    setBufferSize.setText("512");
    int bufferSize = setBufferSize.getText().getIntValue();
    setBufferSize.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(setBufferSize);
    bufferSizeLabel.attachToComponent(&setBufferSize, true);
    addAndMakeVisible(sampleRateLabel);

    setMinCQTfreq.setText("20.0");
    float minCQTfreq = setMinCQTfreq.getText().getFloatValue();
    setMinCQTfreq.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(setMinCQTfreq);
    minCQTfreqLabel.attachToComponent(&setMinCQTfreq, true);
    addAndMakeVisible(bufferSizeLabel);

    setNumCQTbins.setText("128");
    int numCQTbins = setNumCQTbins.getText().getIntValue();
    setNumCQTbins.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(setNumCQTbins);
    numCQTbinsLabel.attachToComponent(&setNumCQTbins, true);
    addAndMakeVisible(minCQTfreqLabel);

    fftOrderSlider.setRange(8, 12, 1);
    fftOrderSlider.setValue(11); // Default to 2048 (2^11) FFT size
    addAndMakeVisible(fftOrderSlider);
    fftOrderLabel.attachToComponent(&fftOrderSlider, true);
    addAndMakeVisible(numCQTbinsLabel);



}

SettingsComponent::~SettingsComponent() = default;

//=============================================================================
void SettingsComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawFittedText("Hi Mckinley",
                     getLocalBounds(),
                     juce::Justification::centred,
                     1);
}

void SettingsComponent::resized() 
{
    auto area = getLocalBounds().reduced(10);
    auto rowHeight = 30;
    area.removeFromLeft(50);



    setSampleRate.setBounds(area.removeFromTop(rowHeight));
    area.removeFromTop(10);

    setBufferSize.setBounds(area.removeFromTop(rowHeight));
    area.removeFromTop(10);

    setMinCQTfreq.setBounds(area.removeFromTop(rowHeight));
    area.removeFromTop(10);

    setNumCQTbins.setBounds(area.removeFromTop(rowHeight));
    area.removeFromTop(10);

    fftOrderSlider.setBounds(area.removeFromTop(rowHeight));
}
