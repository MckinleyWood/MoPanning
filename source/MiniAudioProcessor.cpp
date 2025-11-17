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