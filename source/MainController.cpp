#include "MainController.h"

//=============================================================================
MainController::MainController() = default;
MainController::~MainController() = default;

//=============================================================================
bool MainController::loadFile(const juce::File& f)
{
    return engine.loadFile(f);
}

//=============================================================================
void MainController::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    engine.prepareToPlay(samplesPerBlock, sampleRate);
    // analyzer.prepare(samplesPerBlock, sampleRate); // Not implemented yet
}

void MainController::releaseResources() 
{ 
    engine.releaseResources(); 
}

void MainController::getNextAudioBlock(
    const juce::AudioSourceChannelInfo& info)
{
    engine.getNextAudioBlock(info);
    // analyzer.push(info.buffer); // Not implemented yet
}