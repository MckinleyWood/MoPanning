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

//=============================================================================
using ParamLayout = juce::AudioProcessorValueTreeState::ParameterLayout;

struct ParameterDescriptor
{
    juce::String id;
    juce::String displayName;
    juce::String description;
    enum Type { Float, Choice } type;
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
    void setDefaultParameters();
    bool loadFile(const juce::File& f);
    void togglePlayback();

    std::vector<ParameterDescriptor> getParameterDescriptors() const;
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept;

    std::vector<frequency_band> getLatestResults() const;
    juce::AudioDeviceManager& getDeviceManager();

    void valueTreePropertyChanged(juce::ValueTree&, 
                                  const juce::Identifier& id) override;

private:
    //=========================================================================
    std::unique_ptr<AudioAnalyzer> analyzer;
    std::unique_ptr<MiniAudioProcessor> processor;
    std::unique_ptr<AudioEngine> engine;
    GLVisualizer* visualizer = nullptr;    

    juce::AudioProcessorValueTreeState* apvts;

    std::vector<ParameterDescriptor> parameterDescriptors;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainController)
};