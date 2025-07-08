#include "MainController.h"

//=============================================================================
MainController::MainController()
    : settingsTree(ParamIDs::root)
{
    settingsTree.setProperty(ParamIDs::analysisMode, 0, nullptr);
    settingsTree.setProperty(ParamIDs::fftOrder, 11, nullptr);
    settingsTree.setProperty(ParamIDs::minFrequency, 20.f, nullptr);
    settingsTree.setProperty(ParamIDs::numCQTbins, 256, nullptr);
    settingsTree.setProperty(ParamIDs::recedeSpeed, 5.f, nullptr);
    settingsTree.setProperty(ParamIDs::dotSize, 0.05f,nullptr);
    settingsTree.setProperty(ParamIDs::nearZ, 0.1f, nullptr);
    settingsTree.setProperty(ParamIDs::fadeEndZ, 5.f, nullptr);
    settingsTree.setProperty(ParamIDs::farZ, 100.f, nullptr);
    settingsTree.setProperty(ParamIDs::fov, 45.f, nullptr);

    settingsTree.addListener(this);
    engine.setAudioCallbackSource(this);
}

MainController::~MainController()
{
    settingsTree.removeListener(this);
}

//=============================================================================
void MainController::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    settingsTree.setProperty(ParamIDs::sampleRate, sampleRate, nullptr);
    settingsTree.setProperty(ParamIDs::samplesPerBlock, 
                             samplesPerBlock, nullptr);

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

//=============================================================================
bool MainController::loadFile(const juce::File& f)
{
    return engine.loadFile(f);
}

void MainController::togglePlayback()
{
    engine.togglePlayback();
}

std::vector<frequency_band> MainController::getLatestResults() const
{
    return analyzer.getLatestResults();
}

//=============================================================================
double MainController::getSampleRate() const
{
    return settingsTree[ParamIDs::sampleRate];
}

int MainController::getSamplesPerBlock() const
{
    return settingsTree[ParamIDs::samplesPerBlock];
}

int MainController::getAnalysisMode() const
{
    return settingsTree[ParamIDs::analysisMode];
}

int MainController::getFFTOrder() const
{
    return settingsTree[ParamIDs::fftOrder];
}

float MainController::getMinFrequency() const
{
    return settingsTree[ParamIDs::minFrequency];
}

int MainController::getNumCQTBins() const
{
    return settingsTree[ParamIDs::numCQTbins];
}

float  MainController::getRecedeSpeed() const
{
    return settingsTree[ParamIDs::recedeSpeed];
}

float  MainController::getDotSize() const
{
    return settingsTree[ParamIDs::dotSize];
}

float MainController::getNearZ() const
{
    return settingsTree[ParamIDs::nearZ];
}

float MainController::getFadeEndZ() const
{
    return settingsTree[ParamIDs::fadeEndZ];
}

float MainController::getFarZ() const
{
    return settingsTree[ParamIDs::farZ];
}

float MainController::getFOV() const
{
    return settingsTree[ParamIDs::fov];
}

//=============================================================================
void MainController::setSampleRate(double newSampleRate)
{
    // Dont mess with this (yet)
    // settingsTree.setProperty(ParamIDs::sampleRate, newSampleRate, nullptr);
}

void MainController::setSamplesPerBlock(int newSamplesPerBlock) 
{ 
    // Dont mess with this (yet)
    // settingsTree.setProperty(ParamIDs::samplesPerBlock, 
    //                          newSamplesPerBlock, nullptr); 
}

void MainController::setAnalysisMode(int newAnalysisMode) 
{ 
    settingsTree.setProperty(ParamIDs::analysisMode,    
                             newAnalysisMode, nullptr); 
}

void MainController::setFFTOrder(int newFftOrder) 
{ 
    settingsTree.setProperty(ParamIDs::fftOrder, newFftOrder, nullptr); 
}

void MainController::setMinFrequency(float newMinFrequency) 
{ 
    settingsTree.setProperty(ParamIDs::minFrequency, 
                             newMinFrequency, nullptr); 
}

void MainController::setNumCQTBins(int newNumCQTBins) 
{ 
    settingsTree.setProperty(ParamIDs::numCQTbins, newNumCQTBins, nullptr); 
}

void MainController::setRecedeSpeed(float newRecedeSpeed) 
{ 
    settingsTree.setProperty(ParamIDs::recedeSpeed, newRecedeSpeed, nullptr);
}

void MainController::setDotSize(float newDotSize) 
{ 
    settingsTree.setProperty(ParamIDs::dotSize, newDotSize, nullptr); 
}

void MainController::setNearZ(float newNearZ) 
{ 
    settingsTree.setProperty(ParamIDs::nearZ, newNearZ, nullptr); 
}

void MainController::setFadeEndZ(float newFadeEndZ) 
{ 
    settingsTree.setProperty(ParamIDs::fadeEndZ, newFadeEndZ, nullptr); 
}

void MainController::setFarZ(float newFarZ) 
{ 
    settingsTree.setProperty(ParamIDs::farZ, newFarZ, nullptr); 
}

void MainController::setFOV(float newFov) 
{ 
    settingsTree.setProperty(ParamIDs::fov, newFov, nullptr); 
}

//=============================================================================
void MainController::valueTreePropertyChanged(juce::ValueTree&, 
                                              const juce::Identifier& id)
{
    if (id == ParamIDs::sampleRate)
        analyzer.setSampleRate(getSampleRate());

    else if (id == ParamIDs::samplesPerBlock)
        analyzer.setSamplesPerBlock(getSamplesPerBlock());

    else if (id == ParamIDs::analysisMode)
        analyzer.setAnalysisMode(static_cast<AnalysisMode>(getAnalysisMode()));

    else if (id == ParamIDs::fftOrder)
        analyzer.setFFTOrder(getFFTOrder());

    else if (id == ParamIDs::minFrequency)
        analyzer.setMinFrequency(getMinFrequency());

    else if (id == ParamIDs::numCQTbins)
        analyzer.setNumCQTBins(getNumCQTBins());

    else if (id == ParamIDs::recedeSpeed)
        visualizer->setRecedeSpeed(getRecedeSpeed());

    else if (id == ParamIDs::dotSize)
        visualizer->setDotSize(getDotSize());

    else if (id == ParamIDs::nearZ)
        visualizer->setNearZ(getNearZ());

    else if (id == ParamIDs::fadeEndZ)
        visualizer->setFadeEndZ(getFadeEndZ());

    else if (id == ParamIDs::farZ)
        visualizer->setFarZ(getFarZ());

    else if (id == ParamIDs::fov)
        visualizer->setFOV(getFOV());
}