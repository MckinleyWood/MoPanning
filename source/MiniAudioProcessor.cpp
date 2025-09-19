#include "MiniAudioProcessor.h"

//=============================================================================
MiniAudioProcessor::MiniAudioProcessor(
        juce::AudioProcessorValueTreeState::ParameterLayout&& layout, 
        juce::UndoManager* undoMgr)
    : AudioProcessor(BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
        parameters(*this, undoMgr, "PARAM_TREE", std::move(layout))
{
}

MiniAudioProcessor::~MiniAudioProcessor() = default;

//=============================================================================
void MiniAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void MiniAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi)
{
    juce::ignoreUnused(buffer, midi);
}

void MiniAudioProcessor::setCurrentProgram(int index)  
{
    juce::ignoreUnused(index);
}

const juce::String MiniAudioProcessor::getProgramName(int index)  
{
    juce::ignoreUnused(index);
    return {}; 
}

void MiniAudioProcessor::changeProgramName(int index, 
                                           const juce::String& newName)  
{
    juce::ignoreUnused(index, newName);
}

void MiniAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MiniAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data,
                                                                sizeInBytes));
    if (xmlState != nullptr)
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}