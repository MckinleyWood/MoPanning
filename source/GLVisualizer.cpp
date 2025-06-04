#include "GLVisualizer.h"
#include "MainController.h"

//=============================================================================
GLVisualizer::GLVisualizer(MainController& c) : controller(c) {}

GLVisualizer::~GLVisualizer()
{
    openGLContext.setContinuousRepainting(false);
    openGLContext.detach();
}

//=============================================================================
void GLVisualizer::initialise()  {}
void GLVisualizer::shutdown()    {}

void GLVisualizer::render()
{
    // Clear to black; nothing else drawn yet
    juce::OpenGLHelpers::clear(juce::Colours::black);
}

void GLVisualizer::resized() {}
