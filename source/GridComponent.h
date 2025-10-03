#pragma once
#include <JuceHeader.h>

class MainController;

class GridComponent : public juce::Component
{
public:
    GridComponent(MainController& controllerRef);

    void setFrequencies (std::vector<float> freqs)
    {
        frequencies = std::move(freqs);
        repaint();
    }

    void setMinFrequency (float f) {minFrequency = f; repaint();}
    void setSampleRate (double sr) {sampleRate = sr; repaint();}

    void paint (juce::Graphics& g) override;

private:
    MainController& controller;
    
    std::vector<float> frequencies;
    float minFrequency;
    double sampleRate;
};