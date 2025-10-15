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

    void prepareToPlay(int newSamplesPerBlock, double newSampleRate);

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
    void setFadeEndZ(float newFadeEndZ);

    void paint(juce::Graphics& g) override;

private:
    //=========================================================================
    struct Particle
    {
        float spawnX;
        float spawnY;
        float z; // Current z position
        float spawnAlpha;
        float spawnTime = 0.0f; // Time since app start when particle spawned
    };
    std::deque<Particle> particles; // Queue of particles
    
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    VertexBufferObject vbo; // Vertex buffer object
    GLuint vao = 0; // Vertex-array object
    GLuint instanceVBO = 0; // buffer ID for per-instance data
    struct InstanceData { float x, y, z, spawnAlpha; };

    juce::Vector3D<float> cameraPosition { 0.0f, 0.0f, -2.0f };

    juce::Matrix3D<float> view; // View matrix
    juce::Matrix3D<float> projection; // Projection matrix

    MainController& controller;

    GLuint colourMapTex = 0;
    bool newTextureRequsted = false; // Flag to rebuild texture

    double sampleRate;
    float startTime; // App-launch time in seconds
    float lastFrameTime; // Time of last frame in seconds

    //=========================================================================
    /* Parameters */

    Dimension dimension;
    ColourScheme colourScheme;

    float minFrequency; // Minimum frequency to display (Hz)
    float recedeSpeed; // Speed that objects recede
    float dotSize; // Radius of the dots
    float ampScale; // Amplitude scale factor
    float fadeEndZ; // Distance at which points are fully faded
    float nearZ = 0.1f; // Distance to the start of clip space (m)
    float farZ = 100.f; // Distance to the end of clip space (m)
    float fov = 45.f; // Vertical field of view (degrees)
    int maxParticles = 200000;
    
    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLVisualizer)
};