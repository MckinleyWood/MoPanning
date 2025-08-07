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

//=============================================================================
namespace ParamIDs
{
    static const juce::Identifier root { "Settings" };

    static const juce::Identifier sampleRate { "sampleRate" };
    static const juce::Identifier samplesPerBlock { "samplesPerBlock" };

    static const juce::Identifier inputType { "inputType" };

    static const juce::Identifier transform { "transform" };
    static const juce::Identifier panMethod { "panMethod" };
    static const juce::Identifier fftOrder { "fftOrder" };
    static const juce::Identifier minFrequency { "minFrequency" };
    static const juce::Identifier numCQTbins { "numCQTbins" };

    static const juce::Identifier dimension { "dimension" };
    static const juce::Identifier recedeSpeed { "recedeSpeed" };
    static const juce::Identifier dotSize { "dotSize" };
    static const juce::Identifier ampScale { "ampScale" };
    static const juce::Identifier nearZ { "nearZ" };
    static const juce::Identifier fadeEndZ { "fadeEndZ" };
    static const juce::Identifier farZ { "farZ" };
    static const juce::Identifier fov { "fov" };
    
    // static const juce::Identifier  { "" };
}

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
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped () override;

    void prepareAnalyzer();
    void registerVisualizer(GLVisualizer* v);
    bool loadFile(const juce::File& f);
    void togglePlayback();

    std::vector<frequency_band> getLatestResults() const;
    juce::AudioDeviceManager& getDeviceManager();

    double getSampleRate() const;
    int    getSamplesPerBlock() const;
    int    getInputType() const;
    int    getTransform() const;
    int    getPanMethod() const;
    int    getFFTOrder() const;
    float  getMinFrequency() const;
    int    getNumCQTBins() const;
    int    getDimension() const;
    float  getRecedeSpeed() const;
    float  getDotSize() const;
    float  getAmpScale() const;
    float  getNearZ() const;
    float  getFadeEndZ() const;
    float  getFarZ() const;
    float  getFOV() const;

    void setSampleRate(double newSampleRate);
    void setSamplesPerBlock(int newSamplesPerBlock);
    void setInputType(int newInputType);
    void setTransform(int newTransform);
    void setPanMethod(int newPanMethod);
    void setFFTOrder(int newFftOrder);
    void setMinFrequency(float newMinFrequency);
    void setNumCQTBins(int newNumCQTbins);
    void setDimension(int newDimension);
    void setRecedeSpeed(float newRecedeSpeed);
    void setDotSize(float newDotSize);
    void setAmpScale(float newAmpScale);
    void setNearZ(float newNearZ);
    void setFadeEndZ(float newFadeEndZ);
    void setFarZ(float newFarZ);
    void setFOV(float newFov);

    void valueTreePropertyChanged(juce::ValueTree&, 
                                  const juce::Identifier& id) override;

    AudioAnalyzer& getAnalyzer() { return analyzer; }


private:
    //=========================================================================
    AudioAnalyzer analyzer;
    AudioEngine engine;
    GLVisualizer* visualizer = nullptr;

    juce::ValueTree settingsTree;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainController)
};