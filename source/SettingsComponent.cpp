#include "SettingsComponent.h"
#include "MainController.h"

//=============================================================================
SettingsComponent::SettingsComponent(MainController& c) : controller(c)
{
    // Set up combo boxes
    sampleRateBox.addItem("24,000Hz", 24000);
    sampleRateBox.addItem("44,100Hz", 44100);
    sampleRateBox.addItem("48,000Hz", 48000);
    sampleRateBox.setSelectedId((int)controller.getSampleRate());
    sampleRateBox.addListener(this);

    samplesPerBlockBox.addItem("64", 64);
    samplesPerBlockBox.addItem("128", 128);
    samplesPerBlockBox.addItem("256", 256);
    samplesPerBlockBox.addItem("512", 512);
    samplesPerBlockBox.addItem("1024", 1024);
    samplesPerBlockBox.setSelectedId(controller.getSamplesPerBlock());
    samplesPerBlockBox.addListener(this);

    analysisModeBox.addItem("FFT", 1);
    analysisModeBox.addItem("CQT", 2);
    analysisModeBox.setSelectedId(controller.getAnalysisMode() + 1);
    analysisModeBox.addListener(this);

    fftOrderBox.addItem("8", 8);
    fftOrderBox.addItem("9", 9);
    fftOrderBox.addItem("10", 10);
    fftOrderBox.addItem("11", 11);
    fftOrderBox.setSelectedId(controller.getFFTOrder());
    fftOrderBox.addListener(this);

    minFrequencyBox.addItem("20Hz", 20);
    minFrequencyBox.addItem("50Hz", 50);
    minFrequencyBox.addItem("100Hz", 100);
    minFrequencyBox.setSelectedId((int)controller.getMinFrequency());
    minFrequencyBox.addListener(this);

    numCQTbinsBox.addItem("128", 128);
    numCQTbinsBox.addItem("256", 256);
    numCQTbinsBox.addItem("512", 512);
    numCQTbinsBox.setSelectedId(controller.getNumCQTBins());
    numCQTbinsBox.addListener(this);

    // Set up sliders
    recedeSpeedSlider.setRange(0.1, 20.0);
    recedeSpeedSlider.setValue(controller.getRecedeSpeed());
    recedeSpeedSlider.addListener(this);

    dotSizeSlider.setRange(0.001, 1.0);
    dotSizeSlider.setValue(controller.getDotSize());
    dotSizeSlider.addListener(this);

    nearZSlider.setRange(0.01, 1.0);
    nearZSlider.setValue(controller.getNearZ());
    nearZSlider.addListener(this);

    fadeEndZSlider.setRange(1.0, 100.0);
    fadeEndZSlider.setValue(controller.getFadeEndZ());
    fadeEndZSlider.addListener(this);

    farZSlider.setRange(1.0, 1000.0);
    farZSlider.setValue(controller.getFarZ());
    farZSlider.addListener(this);

    fovSlider.setRange(10.0, 120.0);
    fovSlider.setValue(controller.getFOV());
    fovSlider.addListener(this);

    // Make all components visible
    addAndMakeVisible(title);
    for (const auto& setting : getSettings())
    {
        addAndMakeVisible(setting);
    }
}

SettingsComponent::~SettingsComponent() 
{
    for (const auto& comp : getSettings())
    {
        // Remove listener? Anything I need to do here?
    }
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

//=============================================================================
void SettingsComponent::comboBoxChanged(juce::ComboBox* b)
{
    if (b == &sampleRateBox)
        controller.setSampleRate((double)b->getSelectedId());

    else if (b == &samplesPerBlockBox)
        controller.setSamplesPerBlock(b->getSelectedId());

    else if (b == &analysisModeBox)
        controller.setAnalysisMode(b->getSelectedId() - 1);

    else if (b == &fftOrderBox)       
        controller.setFFTOrder(b->getSelectedId());

    else if (b == &minFrequencyBox)   
        controller.setMinFrequency((float)b->getSelectedId());

    else if (b == &numCQTbinsBox)
        controller.setNumCQTBins(b->getSelectedId());
}

void SettingsComponent::sliderValueChanged(juce::Slider* s)
{
    if (s == &recedeSpeedSlider) 
        controller.setRecedeSpeed((float)s->getValue());

    else if (s == &dotSizeSlider)    
        controller.setDotSize((float)s->getValue());

    else if (s == &nearZSlider)      
        controller.setNearZ((float)s->getValue());

    else if (s == &fadeEndZSlider)      
        controller.setFadeEndZ((float)s->getValue());

    else if (s == &farZSlider)       
        controller.setFarZ((float)s->getValue());

    else if (s == &fovSlider)        
        controller.setFOV((float)s->getValue());
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
