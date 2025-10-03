#include "GridComponent.h"

GridComponent::GridComponent(MainController& controllerRef)
    : controller(controllerRef) {}

void GridComponent::paint (juce::Graphics& g)
{
    g.setColour(juce::Colours::darkgrey);

    if(frequencies.empty())
        return;

    auto bounds = getLocalBounds().toFloat();
    float maxFreq = (float) sampleRate * 0.5f; // Nyquist frequency
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
        g.setFont (12.0f);
        g.drawText (juce::String(f) + "Hz",
                    (int) bounds.getX() + 4,
                    (int) yPix - 8,
                    60, 16,
                    juce::Justification::left);
    }
}