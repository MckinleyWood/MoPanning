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

/* MainController

This file handles the control logic and owns the audio engine, 
transport, and parameters, and supplies data the the other pieces. All
communication between parts of the program must run through here.
*/

#pragma once
#include <JuceHeader.h>
#include "AudioAnalyzer.h"
#include "AudioEngine.h"
#include "GLVisualizer.h"
#include "MiniAudioProcessor.h"
#include "GridComponent.h"
#include "VideoWriter.h"
#include "Utils.h"

//=============================================================================
using ParamLayout = juce::AudioProcessorValueTreeState::ParameterLayout;

struct ParameterDescriptor
{
    juce::String id;
    juce::String displayName;
    juce::String description;
    juce::String group;
    enum Type { Float, Choice, Bool } type;
    float defaultValue;
    juce::NormalisableRange<float> range; // For float parameters
    juce::StringArray choices; // For choice parameters
    juce::String unit;
    std::function<void(float)> onChanged;
    bool display = true;
};

//=============================================================================
class MainController : private juce::AudioIODeviceCallback,
                       private juce::ValueTree::Listener
{
public:
    //=========================================================================
    MainController();
    ~MainController() override;

    void audioDeviceIOCallbackWithContext(
        const float *const *inputChannelData, int numInputChannels,
        float *const *outputChannelData, int numOutputChannels, int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void startAudio();

    static ParamLayout makeParameterLayout(
        const std::vector<ParameterDescriptor>& descriptors);

    void registerVisualizer(GLVisualizer* v);
    void registerGrid(GridComponent* g);
    void setDefaultParameters();
    bool loadFile(const juce::File& f);
    void togglePlayback();

    void updateGridTexture();
    void giveFrameToVideoWriter(const uint8_t* rgb, int numBytes);
    void stopRecording();

    std::vector<ParameterDescriptor> getParameterDescriptors() const;
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept;

    juce::AudioDeviceManager& getDeviceManager();

    int getNumTracks() const { return numTracks; }

    void valueTreePropertyChanged(juce::ValueTree&, 
                                  const juce::Identifier& id) override;

    juce::AudioBuffer<float> buffer;
    std::vector<juce::AudioBuffer<float>> buffers;
    std::vector<float> trackGains;

    std::function<void(int)> onNumTracksChanged;
    std::function<void(int)> onDimChanged;

private:
    //=========================================================================
    double sampleRate;
    int samplesPerBlock;

    std::unique_ptr<AudioAnalyzer> analyzer;
    std::unique_ptr<MiniAudioProcessor> processor;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<VideoWriter> videoWriter;
    GLVisualizer* visualizer = nullptr;
    GridComponent* grid = nullptr;

    juce::AudioProcessorValueTreeState* apvts;
    std::vector<ParameterDescriptor> parameterDescriptors;

    static constexpr int maxNumTracks = 8;
    std::array<TrackSlot, maxNumTracks> analysisResults;

    int numTracks = 1;
    bool threeDim = 1;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainController)
};