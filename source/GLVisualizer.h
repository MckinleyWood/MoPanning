#pragma once
#include <JuceHeader.h>

class MainController;

//=============================================================================
juce::Matrix3D<float> makeOrthoScale(float sx, float sy);

//=============================================================================
/*  Struct representing an OpenGL Vertex Buffer Object. */
struct VertexBufferObject
{
    GLuint id = 0;

    void create(juce::OpenGLContext& context)
    {
        if (id == 0)
            context.extensions.glGenBuffers (1, &id);
    }
    void release(juce::OpenGLContext& context)
    {
        if (id != 0)
        {
            context.extensions.glDeleteBuffers (1, &id);
            id = 0;
        }
    }
    ~VertexBufferObject() = default;
};

//=============================================================================
/*  This is the component for the OpenGL canvas. It will handle 
    rendering the visualization.
*/
class GLVisualizer : public juce::OpenGLAppComponent
{
public:
    //=========================================================================
    explicit GLVisualizer(MainController&);
    ~GLVisualizer() override;

    //=========================================================================
    void initialise() override;
    void shutdown() override;
    void render() override;
    void resized() override;

private:
    //=========================================================================
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    VertexBufferObject vbo; // Vertex buffer object (duh)
    juce::Matrix3D<float> mvp; // Model-View-Projection maxtrix
    GLuint vao = 0; // Vertex-array object
    
    MainController& controller;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLVisualizer)
};