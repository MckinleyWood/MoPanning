#include "SettingsComponent.h"
#include "MainController.h"

//=================================================================
class SettingsComponent::SettingsContentComponent : public juce::Component,
                                                    private juce::ComboBox::Listener,
                                                    private juce::Slider::Listener
{
public:
    SettingsContentComponent(MainController& c) : controller(c)
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

            numCQTbinsBox.addItem("64", 64);
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

            // Set up labels
            sampleRateLabel.setText("Sample Rate", juce::dontSendNotification);
            samplesPerBlockLabel.setText("Samples per Block", juce::dontSendNotification);
            analysisModeLabel.setText("Analysis Mode", juce::dontSendNotification);
            fftOrderLabel.setText("FFT Order", juce::dontSendNotification);
            minFrequencyLabel.setText("Min Frequency", juce::dontSendNotification);
            numCQTbinsLabel.setText("CQT Bin Count", juce::dontSendNotification);
            recedeSpeedLabel.setText("Recede Speed", juce::dontSendNotification);
            dotSizeLabel.setText("Dot Size", juce::dontSendNotification);
            nearZLabel.setText("Near Z", juce::dontSendNotification);
            fadeEndZLabel.setText("Fade End Z", juce::dontSendNotification);
            farZLabel.setText("Far Z", juce::dontSendNotification);
            fovLabel.setText("FOV", juce::dontSendNotification);

            // Make all components visible
            addAndMakeVisible(title);
            for (const auto& setting : getSettings())
            {
                addAndMakeVisible(setting);
            }

            for (auto* label : getLabels())
            {
                label->setJustificationType(juce::Justification::left);
                label->setFont(juce::Font(13.0f));
                addAndMakeVisible(label);
            }

            initialized = true;
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        const int rowHeight = 50;
        const int labelHeight = 14;
        const int numSettings = (int)getSettings().size();

        // Layout the title at the top
        auto titleZone = bounds.removeFromTop(60);
        title.setBounds(titleZone);

        // Layout each setting below the title
        auto settings = getSettings();
        auto labels = getLabels();

        for (int i = 0; i < numSettings; ++i)
        {
            auto row = bounds.removeFromTop(rowHeight);

            auto labelZone = row.removeFromTop(labelHeight);
            labels[i]->setBounds(labelZone);

            settings[i]->setBounds(row.reduced(0, 4));
        }
    }

    void paint(juce::Graphics& g) override
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
        title.setText("Hi Mckinley", juce::dontSendNotification);
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

private:
    MainController& controller;

    juce::Label title;

    // UI Components
    juce::ComboBox sampleRateBox, samplesPerBlockBox, analysisModeBox, fftOrderBox, minFrequencyBox, numCQTbinsBox;
    juce::Slider recedeSpeedSlider, dotSizeSlider, nearZSlider, fadeEndZSlider, farZSlider, fovSlider;

    juce::Label sampleRateLabel, samplesPerBlockLabel, analysisModeLabel, fftOrderLabel,
                minFrequencyLabel, numCQTbinsLabel, recedeSpeedLabel, dotSizeLabel,
                nearZLabel, fadeEndZLabel, farZLabel, fovLabel;

    bool initialized = false;

    // Helpers
    std::vector<juce::Component*> getSettings()
    {
        return {
            &sampleRateBox, &samplesPerBlockBox, &analysisModeBox, &fftOrderBox,
            &minFrequencyBox, &numCQTbinsBox, &recedeSpeedSlider, &dotSizeSlider,
            &nearZSlider, &fadeEndZSlider, &farZSlider, &fovSlider
        };
    }

    std::vector<juce::Label*> getLabels()
    {
        return {
            &sampleRateLabel, &samplesPerBlockLabel, &analysisModeLabel, &fftOrderLabel,
            &minFrequencyLabel, &numCQTbinsLabel, &recedeSpeedLabel, &dotSizeLabel,
            &nearZLabel, &fadeEndZLabel, &farZLabel, &fovLabel
        };
    }

    // Listeners
    void comboBoxChanged(juce::ComboBox* b) override
    {
        if (!initialized) return; // skip early triggers

        if (b == &sampleRateBox)
        {
            controller.setSampleRate((double)b->getSelectedId());
        }
        else if (b == &samplesPerBlockBox)
        {
            controller.setSamplesPerBlock(b->getSelectedId());
        }
        else if (b == &analysisModeBox)
        {
            controller.setAnalysisMode(b->getSelectedId() - 1);
        }
        else if (b == &fftOrderBox)
        {
            controller.setFFTOrder(b->getSelectedId());
        }
        else if (b == &minFrequencyBox)
        {
            controller.setMinFrequency((float)b->getSelectedId());
        }
        else if (b == &numCQTbinsBox)
        {
            controller.setNumCQTBins(b->getSelectedId());
        }

        controller.prepareAnalyzer();
    }

    void sliderValueChanged(juce::Slider* s) override
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
};

//=============================================================================
SettingsComponent::SettingsComponent(MainController& c) : controller(c)
{
    content = std::make_unique<SettingsContentComponent>(controller);
    viewport.setViewedComponent(content.get(), true);
    addAndMakeVisible(viewport);
}

SettingsComponent::~SettingsComponent() 
{
    for (const auto& comp : getSettings())
    {
        // Remove listener? Anything I need to do here?
    }
}

//=============================================================================
void SettingsComponent::resized() 
{
    viewport.setBounds(getLocalBounds());

    // const int totalRows = 12;
    // const int rowHeight = 50;
    // const int titleHeight = 40;
    const int contentHeight = 800;

    if (content)
        content->setSize(getWidth() - 10, contentHeight); // Adjust height as needed
}

//=============================================================================
void SettingsComponent::comboBoxChanged(juce::ComboBox* b)
{
    if (!initialized) return; // skip early triggers

    if (b == &sampleRateBox)
    {
        controller.setSampleRate((double)b->getSelectedId());
        controller.prepareAnalyzer();
    }

    else if (b == &samplesPerBlockBox)
    {
        controller.setSamplesPerBlock(b->getSelectedId());
        controller.prepareAnalyzer();
    }

    else if (b == &analysisModeBox)
    {
        controller.setAnalysisMode(b->getSelectedId() - 1);
    }

    else if (b == &fftOrderBox)       
    {
        controller.setFFTOrder(b->getSelectedId());
        controller.prepareAnalyzer();
    }
    else if (b == &minFrequencyBox)   
    {
        controller.setMinFrequency((float)b->getSelectedId());
        controller.prepareAnalyzer();
    }
    else if (b == &numCQTbinsBox)
    {
        controller.setNumCQTBins(b->getSelectedId());
        controller.prepareAnalyzer();
    }
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

std::vector<juce::Label*> SettingsComponent::getLabels()
{
    return {
        &sampleRateLabel,
        &samplesPerBlockLabel,
        &analysisModeLabel,
        &fftOrderLabel,
        &minFrequencyLabel,
        &numCQTbinsLabel,
        &recedeSpeedLabel,
        &dotSizeLabel,
        &nearZLabel,
        &fadeEndZLabel,
        &farZLabel,
        &fovLabel
    };
}
