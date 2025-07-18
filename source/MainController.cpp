#include "MainController.h"

//=============================================================================
MainController::MainController()
    : settingsTree(ParamIDs::root)
{
    settingsTree.setProperty(ParamIDs::transform, 1, nullptr);
    settingsTree.setProperty(ParamIDs::panMethod, 3, nullptr);
    settingsTree.setProperty(ParamIDs::fftOrder, 11, nullptr);
    settingsTree.setProperty(ParamIDs::minFrequency, 20.f, nullptr);
    settingsTree.setProperty(ParamIDs::numCQTbins, 128, nullptr);
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
    int numCQTbins = getNumCQTBins();
    int fftOrder = getFFTOrder();
    float minFrequency = getMinFrequency();

    settingsTree.setProperty(ParamIDs::sampleRate, sampleRate, nullptr);
    settingsTree.setProperty(ParamIDs::samplesPerBlock, 
                             samplesPerBlock, nullptr);
    settingsTree.setProperty(ParamIDs::numCQTbins, 
                             numCQTbins, nullptr);
    settingsTree.setProperty(ParamIDs::fftOrder, 
                             fftOrder, nullptr);
    settingsTree.setProperty(ParamIDs::minFrequency, 
                             minFrequency, nullptr);
                             
    engine.prepareToPlay(samplesPerBlock, sampleRate);
    analyzer.prepare(samplesPerBlock, sampleRate,
                     numCQTbins, fftOrder,
                     minFrequency);
}

void MainController::prepareAnalyzer()
{
    if (engine.isPlaying())
{
    engine.stopPlayback();
    juce::Thread::sleep(50); // Give time for audio thread to finish
    analyzer.prepare(getSamplesPerBlock(), 
                     getSampleRate(), 
                     getNumCQTBins(),
                     getFFTOrder(),
                     getMinFrequency());
    engine.startPlayback();
}
else
{
    analyzer.prepare(getSamplesPerBlock(), 
                     getSampleRate(), 
                     getNumCQTBins(),
                     getFFTOrder(),
                     getMinFrequency());
}

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

int MainController::getTransform() const
{
    return settingsTree[ParamIDs::transform];
}

int MainController::getPanMethod() const
{
    return settingsTree[ParamIDs::panMethod];
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

void MainController::setTransform(int newTransform) 
{ 
    settingsTree.setProperty(ParamIDs::transform,    
                             newTransform, nullptr); 
}

void MainController::setPanMethod(int newPanMethod) 
{ 
    settingsTree.setProperty(ParamIDs::panMethod,    
                             newPanMethod, nullptr); 
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

    else if (id == ParamIDs::transform)
        analyzer.setTransform(static_cast<Transform>(getTransform()));

    else if (id == ParamIDs::transform)
        analyzer.setPanMethod(static_cast<PanMethod>(getPanMethod()));

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