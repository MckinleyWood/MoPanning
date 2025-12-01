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
/*  This class handles retrieving the audio data from the input file or 
    device.
*/
class AudioEngine
{
public:
    //=========================================================================
    AudioEngine();
    ~AudioEngine();

    void fillAudioBuffers(const float *const *inputChannelData,
                          int numInputChannels,
                          float *const *outputChannelData,
                          int numOutputChannels,
                          int numSamples,
                          juce::AudioBuffer<float>& buffer,
                          bool isFirstTrack,
                          float trackGainIn);

    void setInputType(InputType type);

    bool loadFile(const juce::File&);
    void togglePlayback();

    void prepareToPlay(int samplesPerBlock, double sampleRate);
    void releaseResources();

    bool isPlaying() const;
    void stopPlayback();
    void startPlayback();

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

private:
    //=========================================================================
    juce::AudioDeviceManager deviceManager;
    
    /*  The transport is the timeline controller that owns the file 
        reader source, supplies buffers to the sound card via 
        AudioSourcePlayer, and keeps track of play-head, looping, etc.
    */
    juce::AudioTransportSource transport;

    std::unique_ptr<juce::AudioFormatReaderSource> fileSource;
    juce::AudioFormatManager formatManager;

    InputType inputType;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};