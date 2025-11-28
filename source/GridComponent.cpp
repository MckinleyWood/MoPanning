/*=============================================================================

    This file is part of the MoPanning audio visuaization tool.
    Copyright (C) 2025 Owen Ohlson and Mckinley Wood

    This program is free software: you can redistribute it and/or modify 
    it under the terms of the GNU Affero General Public License as 
    published by the Free Software Foundation, either version 3 of the 
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but 
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
    Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public 
    License along with this program. If not, see 
    <https://www.gnu.org/licenses/>.

=============================================================================*/

#include "GridComponent.h"
#include <cmath>


//=============================================================================
GridComponent::GridComponent(/* MainController& controllerRef */)
    : /* controller(controllerRef), */
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
    /* controller.updateGridTexture(); // Notify controller to update texture */
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