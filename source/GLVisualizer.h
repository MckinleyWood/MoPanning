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
    VertexBufferObject vbo; // Vertex buffer object
    juce::Matrix3D<float> mvp; // Model-View-Projection maxtrix
    GLuint vao = 0; // Vertex-array object

    juce::Vector3D<float> cameraPosition { 0.0f, 0.0f, -2.0f };

    juce::Matrix3D<float> model; // Model matrix
    juce::Matrix3D<float> view; // View matrix
    juce::Matrix3D<float> projection; // Projection matrix

    double startTime = 0.0; // App-launch time in seconds
    float speed = 1.5f; // Speed that objects recede
    float nearZ = 0.1f;
    float fadeEndZ = 10.0f; // Distance until the point fades to black (m)
    float farZ = 100.0f; // Distance to the end of clip space (m)
    float fov = juce::MathConstants<float>::pi / 4.0f;
    
    MainController& controller;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLVisualizer)
};