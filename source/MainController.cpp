#include "MainController.h"

//=============================================================================
MainController::MainController()
    : settingsTree(ParamIDs::root)
{
    settingsTree.setProperty(ParamIDs::sampleRate, 44100., nullptr);
    settingsTree.setProperty(ParamIDs::samplesPerBlock, 512, nullptr);
    settingsTree.setProperty(ParamIDs::analysisMode, 0, nullptr);
    settingsTree.setProperty(ParamIDs::fftOrder, 11, nullptr);
    settingsTree.setProperty(ParamIDs::minFrequency, 20.f, nullptr);
    settingsTree.setProperty(ParamIDs::numCQTbins, 256, nullptr);
    settingsTree.setProperty(ParamIDs::recedeSpeed, 5.f, nullptr);
    settingsTree.setProperty(ParamIDs::dotSize, 0.051f,nullptr);
    settingsTree.setProperty(ParamIDs::nearZ, 0.1f, nullptr);
    settingsTree.setProperty(ParamIDs::fadeEndZ, 5.f, nullptr);
    settingsTree.setProperty(ParamIDs::farZ, 100.f, nullptr);
    settingsTree.setProperty(ParamIDs::fov, 
        juce::MathConstants<float>::pi / 4.0f, nullptr);

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

int MainController::getFftOrder() const
{
    return settingsTree[ParamIDs::fftOrder];
}

float MainController::getMinFrequency() const
{
    return settingsTree[ParamIDs::minFrequency];
}

int MainController::getNumCQTbins() const
{
    return settingsTree[ParamIDs::numCQTbins];
}

double MainController::getStartTime() const
{
    return settingsTree[ParamIDs::startTime];
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

float MainController::getFov() const
{
    return settingsTree[ParamIDs::fov];
}

//=============================================================================
void MainController::setSampleRate(double newSampleRate)
{
    settingsTree.setProperty(ParamIDs::sampleRate, newSampleRate, nullptr);
}

void MainController::setSamplesPerBlock(int newSamplesPerBlock) 
{ 
    settingsTree.setProperty(ParamIDs::samplesPerBlock, 
                             newSamplesPerBlock, nullptr); 
}

void MainController::setAnalysisMode(int newAnalysisMode) 
{ 
    settingsTree.setProperty(ParamIDs::analysisMode, newAnalysisMode, nullptr); 
}

void MainController::setFftOrder(int newFftOrder) 
{ 
    settingsTree.setProperty(ParamIDs::fftOrder, newFftOrder, nullptr); 
}

void MainController::setMinFrequency(float newMinFrequency) 
{ 
    settingsTree.setProperty(ParamIDs::minFrequency, newMinFrequency, nullptr); 
}

void MainController::setNumCQTbins(int newNumCQTbins) 
{ 
    settingsTree.setProperty(ParamIDs::numCQTbins, newNumCQTbins, nullptr); 
}

void MainController::setStartTime(double newStartTime) 
{ 
    settingsTree.setProperty(ParamIDs::startTime, newStartTime, nullptr); 
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

void MainController::setFov(float newFov) 
{ 
    settingsTree.setProperty(ParamIDs::fov, newFov, nullptr); 
}


//=============================================================================
void MainController::valueTreePropertyChanged(juce::ValueTree&, 
                                              const juce::Identifier& id)
{
    if (id == ParamIDs::sampleRate)
        analyzer.setSampleRate(getSampleRate());
    // etc.
}