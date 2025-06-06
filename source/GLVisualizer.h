#pragma once
#include <JuceHeader.h>

class MainController;

//=============================================================================
/*  This is the component for the OpenGL canvas. It will handle redering
    the visualization.
*/
class GLVisualizer : public juce::OpenGLAppComponent
{
public:
    //=========================================================================
    explicit GLVisualizer (MainController&);
    ~GLVisualizer() override;

    //=========================================================================
    void initialise() override;
    void shutdown() override;
    void render() override;
    void resized() override;

private:
    //=========================================================================
    MainController& controller;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLVisualizer)
};