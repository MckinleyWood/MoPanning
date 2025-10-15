#include "GridComponent.h"
#include <cmath>

GridComponent::GridComponent(MainController& controllerRef)
    : controller(controllerRef),
      minFrequency(20.0f),      // default 20 Hz
      sampleRate(48000.0)       // default 48 kHz    
{
    setInterceptsMouseClicks(false, false);
    setAlwaysOnTop(true);
    setOpaque(false);

    // Populate default frequencies immediately
    updateFrequencies();
    // DBG("GridComponent created with minFrequency: " << minFrequency 
    //     << " Hz, sampleRate: " << sampleRate << " Hz");
}

// Call this whenever sampleRate or minFrequency changes
void GridComponent::updateFrequencies()
{
    frequencies.clear();

    float maxFreq = static_cast<float>(sampleRate * 0.5);
    int numLines = 10; // choose how many lines you want

    // Logarithmically spaced frequencies
    float logMin = std::log(minFrequency);
    float logMax = std::log(maxFreq);
    for (int i = 0; i < numLines; ++i)
    {
        float t = static_cast<float>(i) / (numLines - 1);
        float f = std::exp(logMin + t * (logMax - logMin));
        frequencies.push_back(f);
    }

    repaint();
    // DBG("GridComponent::updateFrequencies called. Frequencies updated. Repainted.");
}

void GridComponent::setMinFrequency(float f)
{
    // DBG("GridComponent::setMinFrequency called");
    // DBG(juce::String::formatted("this = %p", this));

    minFrequency = f;
    updateFrequencies();
}

void GridComponent::setSampleRate(double sr)
{
    // DBG("GridComponent::setSampleRate called");
    sampleRate = sr;
    updateFrequencies();
    repaint();
}

void GridComponent::resized()
{
    controller.updateGridTexture(); // Notify controller to update texture
}

void GridComponent::paint(juce::Graphics& g)
{
    // Clear background to transparent
    g.fillAll(juce::Colours::transparentBlack);

    g.setColour(juce::Colours::red);
    // g.setColour(juce::Colours::lightgrey);

    if(frequencies.empty())
    {
        // DBG("GridComponent::paint() skipped — frequencies is empty");
        return;
    }

    auto bounds = getLocalBounds().toFloat();
    float maxFreq = static_cast<float>(sampleRate * 0.5f);
    float logMin = std::log(minFrequency);
    float logMax = std::log(maxFreq);

    for (auto f : frequencies)
    {
        if (f < minFrequency || f > maxFreq)
            continue;

        float norm = (std::log(f) - logMin) / (logMax - logMin);
        float y = juce::jmap(norm, 1.0f, 0.0f);
        float yPix = bounds.getY() + y * bounds.getHeight();

        // Draw horizontal line
        g.drawLine(bounds.getX(), yPix, bounds.getRight(), yPix, 1.0f);

        // Draw frequency label
        g.setFont(12.0f);
        g.drawText(juce::String(f, 0) + "Hz",  // 0 decimals
                   (int)bounds.getX() + 4,
                   (int)yPix - 8,
                   60, 16,
                   juce::Justification::left);
    }
}

void GridComponent::setGridVisible(bool shouldShow)
{
    setVisible(shouldShow);
}