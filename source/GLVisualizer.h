#pragma once
#include <JuceHeader.h>

class GridComponent;
class MainController;

//=============================================================================
enum ColourScheme 
{
    greyscale, 
    rainbow,
    red,
    orange,
    yellow,
    lightGreen,
    darkGreen,
    lightBlue,
    darkBlue,
    purple,
    pink,
    warm,
    cool,
    slider
};
enum Dimension { dimension2, dimension3 };

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
    void initialise() override;
    void shutdown() override;
    void render() override;
    void resized() override;

    //=========================================================================
    void setSampleRate(double newSampleRate);
    void setDimension(Dimension newDimension);
    void setTrackColourScheme(ColourScheme newColourScheme, int trackIndex);
    void setShowGrid(bool shouldShow);
    void setMinFrequency(float newMinFrequency);
    void setRecedeSpeed(float newRecedeSpeed);
    void setDotSize(float newDotSize);
    void setFadeEndZ(float newFadeEndZ);

    //=========================================================================
    void startRecording();
    void stopRecording();

    void paint(juce::Graphics& g) override;

    void createGridImageFromComponent(GridComponent* gridComp);

private:
    //=========================================================================
    void updateParticles();
    void drawParticles(float width, float height, juce::Matrix3D<float>& proj);
    void drawGrid();

    void buildTexture();
    juce::Matrix3D<float> buildProjectionMatrix(float width, float height);
    void updateFBOSize();
    
    //=========================================================================
    struct Particle
    {
        float spawnX;
        float spawnY;
        float z; // Current z position
        float amplitude;
        float spawnTime = 0.0f; // Time since app start when particle spawned
        float trackIndex;
    };

    struct InstanceData { float x, y, z, amplitude; };

    std::deque<Particle> particles; // Queue of particles
    
    std::unique_ptr<juce::OpenGLShaderProgram> mainShader;
    GLuint instanceVBO = 0;
    GLuint mainVAO = 0;
    
    std::unique_ptr<juce::OpenGLShaderProgram> gridShader;
    GLuint gridVBO = 0;
    GLuint gridVAO = 0;

    juce::Image gridImage; 
    juce::OpenGLTexture gridGLTex; 
    std::atomic<bool> gridTextureDirty{false};
    bool gridTextureReady = false;

    juce::Vector3D<float> cameraPosition { 0.0f, 0.0f, -2.0f };
    juce::Matrix3D<float> view; // View matrix
    juce::Matrix3D<float> projection; // Projection matrix
    juce::Matrix3D<float> captureProj;

    // GLuint colourMapTex = 0;
    bool newTextureRequested = true; // Flag to rebuild texture

    float startTime; // App-launch time in seconds
    float lastFrameTime; // Time of last frame in seconds

    juce::OpenGLFrameBuffer captureFBO;
    int captureW = 1280, captureH = 720;
    bool recording;
    std::vector<uint8_t> capturePixels;
    std::vector<uint8_t> flippedPixels; 

    MainController& controller;

    //=========================================================================
    /* Parameters */

    double sampleRate;

    Dimension dimension;
    std::vector<ColourScheme> trackColourSchemes;
    std::vector<GLuint> trackColourTextures;
    
    bool showGrid;
    int numTracks = 1; // Number of tracks in the results vector
    float minFrequency; // Minimum frequency to display (Hz)
    float recedeSpeed; // Speed that objects recede
    float dotSize; // Radius of the dots
    float fadeEndZ; // Distance at which points are fully faded (m)

    float nearZ = 0.1f; // Distance to the start of clip space (m)
    float farZ = 100.f; // Distance to the end of clip space (m)
    float fov = 45.f; // Vertical field of view (degrees)
    int maxParticles = 200000;
    
    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLVisualizer)
};