#include "MainController.h"

//=============================================================================
MainController::MainController()
    : settingsTree(ParamIDs::root)
{
    // Initialize settings tree with default values
    settingsTree.setProperty(ParamIDs::inputType, 1, nullptr);
    settingsTree.setProperty(ParamIDs::transform, 1, nullptr);
    settingsTree.setProperty(ParamIDs::panMethod, 0, nullptr);
    settingsTree.setProperty(ParamIDs::fftOrder, 11, nullptr);
    settingsTree.setProperty(ParamIDs::numCQTbins, 128, nullptr);
    settingsTree.setProperty(ParamIDs::minFrequency, 20.f, nullptr);
    settingsTree.setProperty(ParamIDs::maxAmplitude, 1.f, nullptr);
    settingsTree.setProperty(ParamIDs::dimension, 1, nullptr);
    settingsTree.setProperty(ParamIDs::colourScheme, 1, nullptr);
    settingsTree.setProperty(ParamIDs::recedeSpeed, 5.f, nullptr);
    settingsTree.setProperty(ParamIDs::dotSize, 0.5f,nullptr);
    settingsTree.setProperty(ParamIDs::ampScale, 5.f, nullptr);
    settingsTree.setProperty(ParamIDs::nearZ, 0.1f, nullptr);
    settingsTree.setProperty(ParamIDs::fadeEndZ, 5.f, nullptr);
    settingsTree.setProperty(ParamIDs::farZ, 100.f, nullptr);
    settingsTree.setProperty(ParamIDs::fov, 45.f, nullptr);

    settingsTree.addListener(this);

    auto& dm = engine.getDeviceManager();
    dm.addAudioCallback(this);
    dm.initialise(2, 2, nullptr, true);
}

MainController::~MainController()
{
    settingsTree.removeListener(this);
    auto& dm = engine.getDeviceManager();
    dm.removeAudioCallback(this);
}

//=============================================================================
void MainController::audioDeviceIOCallbackWithContext(
    const float *const *inputChannelData, int numInputChannels,
    float *const *outputChannelData, int numOutputChannels, int numSamples,
    const juce::AudioIODeviceCallbackContext& context)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    
    // Delegate to the audio engine
    engine.fillAudioBuffers(inputChannelData, numInputChannels,
                            outputChannelData, numOutputChannels,
                            numSamples, buffer);

    // Pass the buffer to the analyzer
    analyzer.enqueueBlock(&buffer);

    juce::ignoreUnused(context);
}

void MainController::audioDeviceAboutToStart(juce::AudioIODevice* device) 
{
    double sampleRate = device->getCurrentSampleRate();
    int samplesPerBlock = device->getCurrentBufferSizeSamples();

    settingsTree.setProperty(ParamIDs::sampleRate, sampleRate, nullptr);
    settingsTree.setProperty(ParamIDs::samplesPerBlock, 
                             samplesPerBlock, nullptr);
    
    engine.setInputType(static_cast<InputType>(getInputType()));
    engine.prepareToPlay(samplesPerBlock, sampleRate);
    
    prepareAnalyzer();
}

void MainController::audioDeviceStopped() 
{
    engine.releaseResources(); 
}

//=============================================================================
void MainController::prepareAnalyzer()
{
    // Set analyzer parameters from settings tree
    analyzer.setSampleRate(getSampleRate()); 
    analyzer.setSamplesPerBlock(getSamplesPerBlock()); 
    analyzer.setTransform(static_cast<Transform>(getTransform())); 
    analyzer.setPanMethod(static_cast<PanMethod>(getPanMethod()));
    analyzer.setFFTOrder(getFFTOrder()); 
    analyzer.setNumCQTBins(getNumCQTBins()); 
    analyzer.setMinFrequency(getMinFrequency());
    analyzer.setMaxAmplitude(getMaxAmplitude()); 
    analyzer.prepare();
}

void MainController::registerVisualizer(GLVisualizer* v)
{
    visualizer = v;

    // Push default settings
    visualizer->setDimension(static_cast<Dimension>(getDimension()));
    visualizer->setColourScheme(static_cast<ColourScheme>(getColourScheme()));
    visualizer->setMinFrequency(getMinFrequency());
    visualizer->setRecedeSpeed(getRecedeSpeed());
    visualizer->setDotSize(getDotSize());
    visualizer->setAmpScale(getAmpScale());
    visualizer->setNearZ(getNearZ());
    visualizer->setFadeEndZ(getFadeEndZ());
    visualizer->setFarZ(getFarZ());
    visualizer->setFOV(getFOV());
}

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

juce::AudioDeviceManager& MainController::getDeviceManager()
{
    return engine.getDeviceManager();
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

int MainController::getInputType() const
{
    return settingsTree[ParamIDs::inputType];
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

int MainController::getNumCQTBins() const
{
    return settingsTree[ParamIDs::numCQTbins];
}

float MainController::getMinFrequency() const
{
    return settingsTree[ParamIDs::minFrequency];
}

float MainController::getMaxAmplitude() const
{
    return settingsTree[ParamIDs::maxAmplitude];
}

int MainController::getDimension() const
{
    return settingsTree[ParamIDs::dimension];
}

int MainController::getColourScheme() const
{
    return settingsTree[ParamIDs::colourScheme];
}

float  MainController::getRecedeSpeed() const
{
    return settingsTree[ParamIDs::recedeSpeed];
}

float  MainController::getDotSize() const
{
    return settingsTree[ParamIDs::dotSize];
}

float MainController::getAmpScale() const
{
    return settingsTree[ParamIDs::ampScale];
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
    juce::ignoreUnused(newSampleRate);
}

void MainController::setSamplesPerBlock(int newSamplesPerBlock) 
{ 
    // Dont mess with this (yet)
    // settingsTree.setProperty(ParamIDs::samplesPerBlock, 
    //                          newSamplesPerBlock, nullptr); 
    juce::ignoreUnused(newSamplesPerBlock);
}

void MainController::setInputType(int newInputType) 
{ 
    settingsTree.setProperty(ParamIDs::inputType, newInputType, nullptr); 
}

void MainController::setTransform(int newTransform) 
{ 
    settingsTree.setProperty(ParamIDs::transform, newTransform, nullptr); 
}

void MainController::setPanMethod(int newPanMethod) 
{ 
    settingsTree.setProperty(ParamIDs::panMethod, newPanMethod, nullptr); 
}

void MainController::setFFTOrder(int newFftOrder) 
{ 
    settingsTree.setProperty(ParamIDs::fftOrder, newFftOrder, nullptr); 
}

void MainController::setNumCQTBins(int newNumCQTBins) 
{ 
    settingsTree.setProperty(ParamIDs::numCQTbins, newNumCQTBins, nullptr); 
}

void MainController::setMinFrequency(float newMinFrequency) 
{ 
    settingsTree.setProperty(ParamIDs::minFrequency, 
                             newMinFrequency, nullptr); 
}

void MainController::setMaxAmplitude(float newMaxAmplitude) 
{ 
    settingsTree.setProperty(ParamIDs::maxAmplitude, 
                             newMaxAmplitude, nullptr); 
}

void MainController::setDimension(int newDimension) 
{ 
    settingsTree.setProperty(ParamIDs::dimension, newDimension, nullptr); 
}

void MainController::setColourScheme(int newColourScheme) 
{ 
    settingsTree.setProperty(ParamIDs::colourScheme, 
                             newColourScheme, nullptr); 
}

void MainController::setRecedeSpeed(float newRecedeSpeed) 
{ 
    settingsTree.setProperty(ParamIDs::recedeSpeed, newRecedeSpeed, nullptr);
}

void MainController::setDotSize(float newDotSize) 
{ 
    settingsTree.setProperty(ParamIDs::dotSize, newDotSize, nullptr); 
}

void MainController::setAmpScale(float newAmpScale) 
{ 
    settingsTree.setProperty(ParamIDs::ampScale, newAmpScale, nullptr); 
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

    else if (id == ParamIDs::inputType)
        engine.setInputType(static_cast<InputType>(getInputType()));

    else if (id == ParamIDs::transform)
        analyzer.setTransform(static_cast<Transform>(getTransform()));

    else if (id == ParamIDs::panMethod)
        analyzer.setPanMethod(static_cast<PanMethod>(getPanMethod()));

    else if (id == ParamIDs::fftOrder)
        analyzer.setFFTOrder(getFFTOrder());
        
    else if (id == ParamIDs::numCQTbins)
        analyzer.setNumCQTBins(getNumCQTBins());

    else if (id == ParamIDs::minFrequency)
    {
        analyzer.setMinFrequency(getMinFrequency());
        visualizer->setMinFrequency(getMinFrequency());
    }

    else if (id == ParamIDs::maxAmplitude)
        analyzer.setMaxAmplitude(getMaxAmplitude());

    else if (id == ParamIDs::dimension)
        visualizer->setDimension(static_cast<Dimension>(getDimension()));
    
    else if (id == ParamIDs::colourScheme)
        visualizer->setColourScheme(
            static_cast<ColourScheme>(getColourScheme()));

    else if (id == ParamIDs::recedeSpeed)
        visualizer->setRecedeSpeed(getRecedeSpeed());

    else if (id == ParamIDs::dotSize)
        visualizer->setDotSize(getDotSize());

    else if (id == ParamIDs::ampScale)
        visualizer->setAmpScale(getAmpScale());

    else if (id == ParamIDs::nearZ)
        visualizer->setNearZ(getNearZ());

    else if (id == ParamIDs::fadeEndZ)
        visualizer->setFadeEndZ(getFadeEndZ());

    else if (id == ParamIDs::farZ)
        visualizer->setFarZ(getFarZ());

    else if (id == ParamIDs::fov)
        visualizer->setFOV(getFOV());
}