#pragma once
#include <JuceHeader.h>

class MainController;

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

enum ColourScheme { greyscale, rainbow };
enum Dimension { Dim2D, Dim3D };

//=============================================================================
/*  This is the component for the OpenGL canvas. It handles rendering 
    the visualization.
*/
class GLVisualizer : public juce::OpenGLAppComponent
{
public:
    //=========================================================================
    explicit GLVisualizer(MainController&);
    ~GLVisualizer() override;

    //=========================================================================
    void buildTexture();

    void initialise() override;
    void shutdown() override;
    void render() override;
    void resized() override;

    //=========================================================================
    void setDimension(Dimension newDimension);
    void setColourScheme(ColourScheme newColourScheme);
    void setMinFrequency(float newMinFrequency);
    void setRecedeSpeed(float newRecedeSpeed);
    void setDotSize(float newDotSize);
    void setAmpScale(float newAmpScale);
    void setNearZ(float newNearZ);
    void setFadeEndZ(float newFadeEndZ);
    void setFarZ(float newFarZ);
    void setFOV(float newFOV);

private:
    //=========================================================================
    struct Particle
    {
        float spawnX;
        float spawnY;
        float spawnTime = 0.0f; // Time since app start when particle spawned
        float spawnAlpha;
    };
    std::deque<Particle> particles; // Queue of particles
    
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    VertexBufferObject vbo; // Vertex buffer object
    GLuint vao = 0; // Vertex-array object
    GLuint instanceVBO = 0; // buffer ID for per-instance data
    struct InstanceData { float x, y, spawnTime, spawnAlpha; };

    juce::Vector3D<float> cameraPosition { 0.0f, 0.0f, -2.0f };

    juce::Matrix3D<float> view; // View matrix
    juce::Matrix3D<float> projection; // Projection matrix

    MainController& controller;

    GLuint colourMapTex = 0;

    double startTime; // App-launch time in seconds

    Dimension dimension;
    ColourScheme colourScheme;

    bool newTextureRequsted = false; // Flag to rebuild texture

    float minFrequency;
    float recedeSpeed; // Speed that objects recede
    float dotSize; // Radius of the dots
    float ampScale; // Amplitude scale factor
    float nearZ;
    float fadeEndZ;
    float farZ; // Distance to the end of clip space (m)
    float fov;
    int maxParticles = 200000;
    
    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLVisualizer)
};