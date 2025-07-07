#include "SettingsComponent.h"
#include "MainController.h"

//=============================================================================
SettingsComponent::SettingsComponent(MainController& c) : controller(c)
{
    juce::Font font;
    font.setTypefaceName("Comic Sans MS");
    font.setHeight(18.0f);
    font.setStyleFlags(juce::Font::plain);
    
    setSampleRate.setText("44100.0");
    double sampleRate = setSampleRate.getText().getDoubleValue();
    setSampleRate.setColour(juce::Label::textColourId, juce::Colours::white);

    sampleRateLabel.setFont(font);
    sampleRateLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(setSampleRate);
    addAndMakeVisible(sampleRateLabel);

    setBufferSize.setText("512");
    int bufferSize = setBufferSize.getText().getIntValue();
    setBufferSize.setColour(juce::Label::textColourId, juce::Colours::white);

    bufferSizeLabel.setFont(font);
    bufferSizeLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(setBufferSize);
    addAndMakeVisible(bufferSizeLabel);

    setMinCQTfreq.setText("20.0");
    float minCQTfreq = setMinCQTfreq.getText().getFloatValue();
    setMinCQTfreq.setColour(juce::Label::textColourId, juce::Colours::white);

    minCQTfreqLabel.setFont(font);
    minCQTfreqLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(setMinCQTfreq);
    addAndMakeVisible(minCQTfreqLabel);

    setNumCQTbins.setText("128");
    int numCQTbins = setNumCQTbins.getText().getIntValue();
    setNumCQTbins.setColour(juce::Label::textColourId, juce::Colours::white);

    numCQTbinsLabel.setFont(font);
    numCQTbinsLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(setNumCQTbins);
    addAndMakeVisible(numCQTbinsLabel);

    fftOrderSlider.setRange(8, 12, 1);
    fftOrderSlider.setValue(11); // Default to 2048 (2^11) FFT size

    fftOrderLabel.setFont(font);
    fftOrderLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(fftOrderSlider);
    addAndMakeVisible(fftOrderLabel);

    speedSlider.setRange(0.1f, 20.f);
    speedSlider.setValue(5.f); // Default to 2048 (2^11) FFT size

    speedLabel.setFont(font);
    speedLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(speedSlider);
    addAndMakeVisible(speedLabel);

    speedSlider.addListener(this);

}

SettingsComponent::~SettingsComponent()
{
    speedSlider.removeListener(this);
}

//=============================================================================
void SettingsComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    g.setColour(juce::Colours::white);
    juce::Font font;
    font.setTypefaceName("Comic Sans MS");
    font.setHeight(20.0f);
    font.setStyleFlags(juce::Font::plain);

    g.setFont(font);

    g.drawFittedText("Hi Owen", getLocalBounds(), 
                     juce::Justification::centred, 1);
}

void SettingsComponent::resized() 
{
    auto area = getLocalBounds().reduced(10);
    int rowHeight = 30;

    auto labelArea = area;
    area.removeFromLeft(180);

    setSampleRate.setBounds(area.removeFromTop(rowHeight));
    sampleRateLabel.setBounds(labelArea.removeFromTop(rowHeight));
    area.removeFromTop(10);
    labelArea.removeFromTop(10);

    setBufferSize.setBounds(area.removeFromTop(rowHeight));
    bufferSizeLabel.setBounds(labelArea.removeFromTop(rowHeight));
    area.removeFromTop(10);
    labelArea.removeFromTop(10);

    setMinCQTfreq.setBounds(area.removeFromTop(rowHeight));
    minCQTfreqLabel.setBounds(labelArea.removeFromTop(rowHeight));
    area.removeFromTop(10);
    labelArea.removeFromTop(10);

    setNumCQTbins.setBounds(area.removeFromTop(rowHeight));
    numCQTbinsLabel.setBounds(labelArea.removeFromTop(rowHeight));
    area.removeFromTop(10);
    labelArea.removeFromTop(10);

    auto sliderArea = area;
    sliderArea = sliderArea.withX(sliderArea.getX() - 100)
                       .withWidth(sliderArea.getWidth() + 100);
    fftOrderSlider.setBounds(sliderArea.removeFromTop(rowHeight));
    fftOrderLabel.setBounds(labelArea.removeFromTop(rowHeight));
    area.removeFromTop(rowHeight + 10);
    labelArea.removeFromTop(10);

    sliderArea = area;
    sliderArea = sliderArea.withX(sliderArea.getX() - 100)
                       .withWidth(sliderArea.getWidth() + 100);
    speedSlider.setBounds(sliderArea.removeFromTop(rowHeight));
    speedLabel.setBounds(labelArea.removeFromTop(rowHeight));
}


void SettingsComponent::sliderValueChanged(juce::Slider* s)
{
    if (s == &speedSlider)
        controller.setRecedeSpeed((float)s->getValue());
}