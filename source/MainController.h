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

    static const juce::Identifier transform { "transform" };
    static const juce::Identifier panMethod { "panMethod" };
    static const juce::Identifier fftOrder { "fftOrder" };
    static const juce::Identifier minFrequency { "minFrequency" };
    static const juce::Identifier numCQTbins { "numCQTbins" };

    static const juce::Identifier recedeSpeed { "recedeSpeed" };
    static const juce::Identifier dotSize { "dotSize" };
    static const juce::Identifier nearZ { "nearZ" };
    static const juce::Identifier fadeEndZ { "fadeEndZ" };
    static const juce::Identifier farZ { "farZ" };
    static const juce::Identifier fov { "fov" };
    
    // static const juce::Identifier  { "" };
}

//=============================================================================
class MainController : public juce::AudioSource,
                       private juce::ValueTree::Listener
{
public:
    //=========================================================================
    MainController();
    ~MainController() override;

    void registerVisualizer(GLVisualizer* v) noexcept { visualizer = v; }

    void prepareToPlay(int samplesPerBlock, double sampleRate) override;
    void prepareAnalyzer();
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

    bool loadFile(const juce::File& f);
    void togglePlayback();

    std::vector<frequency_band> getLatestResults() const;

    double getSampleRate() const;
    int    getSamplesPerBlock() const;
    int    getTransform() const;
    int    getPanMethod() const;
    int    getFFTOrder() const;
    float  getMinFrequency() const;
    int    getNumCQTBins() const;
    float  getRecedeSpeed() const;
    float  getDotSize() const;
    float  getNearZ() const;
    float  getFadeEndZ() const;
    float  getFarZ() const;
    float  getFOV() const;

    void setSampleRate(double newSampleRate);
    void setSamplesPerBlock(int newSamplesPerBlock);
    void setTransform(int newTransform);
    void setPanMethod(int newPanMethod);
    void setFFTOrder(int newFftOrder);
    void setMinFrequency(float newMinFrequency);
    void setNumCQTBins(int newNumCQTbins);
    void setRecedeSpeed(float newRecedeSpeed);
    void setDotSize(float newDotSize);
    void setNearZ(float newNearZ);
    void setFadeEndZ(float newFadeEndZ);
    void setFarZ(float newFarZ);
    void setFOV(float newFov);

    void valueTreePropertyChanged(juce::ValueTree&, 
                                  const juce::Identifier& id) override;

private:
    //=========================================================================
    AudioAnalyzer analyzer;
    AudioEngine engine;
    GLVisualizer* visualizer = nullptr;

    juce::ValueTree settingsTree;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainController)
};