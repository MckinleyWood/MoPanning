#include "MainController.h"

//=============================================================================
MainController::MainController()
{
    engine.setAudioCallbackSource(this);
};

MainController::~MainController() = default;

//=============================================================================
bool MainController::loadFile(const juce::File& f)
{
    return engine.loadFile(f);
}

void MainController::togglePlayback()
{
    engine.togglePlayback();
}

std::vector<std::vector<float>> MainController::getLatestCQTMagnitudes() const
{
    return analyzer.getLatestCQTMagnitudes();
}
std::vector<float> MainController::getLatestCombinedPanning() const
{
    return analyzer.getLatestCombinedPanning();
}

//=============================================================================
void MainController::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    this->sampleRate = sampleRate;

    engine.prepareToPlay(samplesPerBlock, sampleRate);
    analyzer.prepare(samplesPerBlock, sampleRate);
}

void MainController::releaseResources() 
{ 
    engine.releaseResources(); 
}

void MainController::getNextAudioBlock(
    const juce::AudioSourceChannelInfo& info)
{
    engine.getNextAudioBlock(info);
    analyzer.enqueueBlock(info.buffer);
}