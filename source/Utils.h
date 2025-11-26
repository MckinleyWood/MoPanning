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

/*  This file holds enums and constants that all MoPanning files can use.
*/

//=============================================================================
namespace Constants
{
    constexpr int maxTracks = 8;
}

//=============================================================================
/*  Contains the data ascociated with one frequency band. 
*/
struct FrequencyBand 
{
    float frequency; // Band frequency in Hertz
    float amplitude; // 'Percieved' amplitude in range [0,1]
    float panIndex; // 'Percieved' lateralization in range [-1,1]
    int trackIndex;  // Which track this band belongs to
};

/*  A double-buffered slot for storing one track's analysis results.
*/
struct TrackSlot
{
    std::array<std::vector<FrequencyBand>, 2> buffers;
    std::atomic<int> activeIndex { 0 }; // Which buffer the reader should use
};


//=============================================================================
/*  Enums for parameter choices */

enum ColourScheme 
{
    greyscale, 
    rainbow,
    red,
    orange,
    yellow,
    lightGreen,
    darkGreen,
    lightBlue,
    darkBlue,
    purple,
    pink,
    warm,
    cool,
    slider
};

enum Dimension 
{ 
    twoD,
    threeD
};