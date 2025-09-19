#pragma once
#include <JuceHeader.h>

//=============================================================================
class MiniAudioProcessor : public juce::AudioProcessor
{
public:
    //=========================================================================
    // constructs APVTS from descriptors
    MiniAudioProcessor(
        juce::AudioProcessorValueTreeState::ParameterLayout&& layout, 
        juce::UndoManager* undoMgr = nullptr);

    ~MiniAudioProcessor() override;

    //=========================================================================
    // AudioProcessor overrides
    const juce::String getName() const override 
    { 
        return "MiniAudioProcessor"; 
    }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer, 
                      juce::MidiBuffer& midiMessages) override;

    // State management (APVTS-backed)
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //=========================================================================
    // Expose APVTS
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept 
    { 
        return parameters; 
    }

private:
    juce::AudioProcessorValueTreeState parameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiniAudioProcessor)
};
