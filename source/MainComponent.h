#pragma once

/* CMake builds don't use an AppConfig.h, so it's safe to include juce 
module headers directly. */
#include <JuceHeader.h>

//======================================================================
/* This component lives inside our window, and this is where you should 
put all your controls and content. */
class MainComponent final : public juce::Component,
                            public juce::AudioIODeviceCallback,
                            public juce::Timer
{
public:
    //==================================================================
    MainComponent();

    //==================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    //==================================================================
    // Audio callback methods
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                        int numInputChannels,
                                        float* const* outputChannelData,
                                        int numOutputChannels,
                                        int numSamples,
                                        const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    //==================================================================

    void timerCallback() override;


private:
    //==================================================================
    // Private member variables go here...

    juce::AudioBuffer<float> analysisBuffer;
    int analysisBufferWritePos = 0;
    static constexpr int analysisBufferSize = 2048;

    static constexpr int fftOrder = 11; // 2^11 = 2048-point fft
    static constexpr int fftSize = 1 << fftOrder; 
    juce::dsp::FFT fft {fftOrder};
    juce::dsp::WindowingFunction<float> window {fftSize, juce::dsp::WindowingFunction<float>::hann };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
