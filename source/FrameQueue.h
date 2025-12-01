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


//=============================================================================
/*  A helper for managing a queue of rgb video frames to pass between threads.
*/
class FrameQueue
{
public:
    //=========================================================================
    FrameQueue()
    {
        // Initialize video FIFO storage
        storage.reserve(bufferSize);
        for (int i = 0; i < bufferSize; ++i)
            storage.push_back(std::make_unique<uint8_t[]>(Constants::frameBytes));
    }

    //=========================================================================
    /*  Adds a frame to the queue.

        This function copies numbytes of data to the next available 
        buffer slot. It returns false if there are no slots available.
    */
    bool enqueueVideoFrame(const uint8_t* rgb, int numBytes)
    {
        // Get the next write position
        const auto scope = abstractFifo.write(1);

        // Copy the frame data into the FIFO storage
        if (scope.blockSize1 > 0)
        {
            // This does not support dynamic frame sizes at the moment
            jassert(numBytes == Constants::frameBytes);

            std::memcpy(storage[scope.startIndex1].get(), rgb, (size_t)numBytes);
            return true;
        }

        return false;
    }
    
    /*  Returns a pointer to the start of the next available buffer to read.

        The caller is reponsible for copying the data out, and must call
        finishedRead() when they are done with it. The function will
        return nullptr if there is no available buffer.
    */
    uint8_t* readNextBuffer()
    {
        int start1, size1, start2, size2;
        abstractFifo.prepareToRead(1, start1, size1, start2, size2);

        if (size1 > 0)
            return storage[start1].get();;
        
        return nullptr; // Queue is empty
    }

    /*  Finalizes a read and advances the internal read pointer.

        This function must be called after each read is finished.
    */
    void finishRead()
    {
        abstractFifo.finishedRead(1);
    }

private:
    //=========================================================================
    static constexpr int bufferSize = 8;

    juce::AbstractFifo abstractFifo { bufferSize };
    std::vector<std::unique_ptr<uint8_t[]>> storage;
};