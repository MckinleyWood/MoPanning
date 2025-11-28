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
#include "Utils.h"

class VideoFrameQueue
{
public:
    //=========================================================================
    VideoFrameQueue() : videoFIFOManager(bufferSize)
    {
        // Initialize video FIFO storage
        videoFIFOStorage.reserve(bufferSize);
        for (int i = 0; i < bufferSize; ++i)
            videoFIFOStorage.push_back(std::make_unique<uint8_t[]>(Constants::frameBytes));
    }

    //=========================================================================
    void enqueueVideoFrame(const uint8_t* rgb, int numBytes)
    {
        // Get the next write position and advance the write pointer
        const auto scope = videoFIFOManager.write(1);

        // Copy the frame data into the FIFO storage
        if (scope.blockSize1 > 0)
            std::memcpy(videoFIFOStorage[scope.startIndex1].get(), rgb, (size_t)numBytes);
    }
    
    //=========================================================================
    // Returns index of buffer to read, or -1 if empty
    int getReadableBufferIndex()
    {
        int start1, size1, start2, size2;
        videoFIFOManager.prepareToRead(1, start1, size1, start2, size2);

        if (size1 > 0)
            return start1;
        
        return -1; // Queue is empty
    }
    
    // Returns reference to specific buffer
    const uint8_t* getBuffer(int index) const
    {
        return videoFIFOStorage[index].get();
    }

    void finishRead(int numRead)
    {
        videoFIFOManager.finishedRead(numRead);
    }

private:
    static constexpr int bufferSize = 8;

    juce::AbstractFifo videoFIFOManager { bufferSize };
    std::vector<std::unique_ptr<uint8_t[]>> videoFIFOStorage;
};