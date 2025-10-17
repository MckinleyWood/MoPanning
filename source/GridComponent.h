#pragma once
#include <JuceHeader.h>
#include "MainController.h"

class GridComponent : public juce::Component
{
public:
    GridComponent(MainController& controllerRef);

    void setMinFrequency(float f);
    void setSampleRate(double sr);
    void updateFrequencies();

    void resized() override;
    void paint (juce::Graphics& g) override;

    void setGridVisible(bool shouldShow);

private:
    MainController& controller;

    std::vector<float> frequencies;
    float minFrequency;
    double sampleRate;
};