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

#pragma once
#include <JuceHeader.h>


//=============================================================================
/*  A frequency grid overlay to put on top of the visualization.
*/
class GridComponent : public juce::Component
{
public:
//=========================================================================
    GridComponent() = default;
    ~GridComponent() = default;

    void setFrequencyRange(float min, float max);
    void updateFrequencies();

    void paint (juce::Graphics& g) override;

private:
    std::vector<float> mainGridlineFrequencies;
    std::vector<float> smolGridlineFrequencies;
    float minFrequency;
    float maxFrequency;
};