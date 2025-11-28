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


//=============================================================================
void GridComponent::updateFrequencies()
{
    mainGridlineFrequencies.clear();
    smolGridlineFrequencies.clear();

    // // Logarithmically spaced frequencies
    // int numLines = 10; // choose how many lines you want
    // float logMin = std::log(minFrequency);
    // float logMax = std::log(maxFrequency);
    // for (int i = 0; i < numLines; ++i)
    // {
    //     float t = (float)(i + 0.5) / (numLines + 0.5);
    //     float f = std::exp(logMin + t * (logMax - logMin));
    //     frequencies.push_back(f);
    // }

    // Good constant frequency values
    mainGridlineFrequencies = 
    {
        10,
        20,
        50,
        100,
        200,
        500,
        1000,
        2000,
        5000,
        10000,
        20000
    };

    smolGridlineFrequencies = 
    {
        30, 40, 60, 70, 80, 90,
        300, 400, 600, 700, 800, 900,
        3000, 4000, 6000, 7000, 8000, 9000,
    };

    repaint();
}

void GridComponent::setFrequencyRange(float min, float max)
{
    minFrequency = min;
    maxFrequency = max;
    updateFrequencies();
}

void GridComponent::paint(juce::Graphics& g)
{
    // Clear background to transparent
    g.fillAll(juce::Colours::transparentBlack);

    // g.setColour(juce::Colours::red);
    g.setColour(juce::Colours::lightgrey);

    if(mainGridlineFrequencies.empty())
    {
        return;
    }

    auto bounds = getLocalBounds().toFloat();
    float logMin = std::log(minFrequency);
    float logMax = std::log(maxFrequency);

    for (float f : mainGridlineFrequencies)
    {
        if (f <= minFrequency || f >= maxFrequency)
            continue;

        float norm = (std::log(f) - logMin) / (logMax - logMin);
        float y = juce::jmap(norm, 1.0f, 0.0f);
        float yPix = bounds.getY() + y * bounds.getHeight();

        // Draw horizontal line
        g.drawLine(bounds.getX(), yPix, bounds.getRight(), yPix, 1.0f);

        // Draw frequency label
        g.setFont(12.0f);
        g.drawText(juce::String(f, 0) + " Hz",  // 0 decimals
                   (int)bounds.getX() + 4,
                   (int)yPix - 16,
                   60, 16,
                   juce::Justification::left);
    }

    for (float f : smolGridlineFrequencies)
    {
        if (f <= minFrequency || f >= maxFrequency)
            continue;

        float norm = (std::log(f) - logMin) / (logMax - logMin);
        float y = juce::jmap(norm, 1.0f, 0.0f);
        float yPix = bounds.getY() + y * bounds.getHeight();

        // Draw horizontal line
        g.drawLine(bounds.getX(), yPix, bounds.getRight(), yPix, 0.2f);
    }
}