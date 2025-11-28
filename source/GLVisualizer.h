/*=============================================================================

    This file is part of the MoPanning audio visuaization tool.
    Copyright (C) 2025 Owen Ohlson and Mckinley Wood

    This program is free software: you can redistribute it and/or modify 
    it under the terms of the GNU Affero General Public License as 
    published by the Free Software Foundation, either version 3 of the 
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but 
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
    Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public 
    License along with this program. If not, see 
    <https://www.gnu.org/licenses/>.

=============================================================================*/

#pragma once
#include <JuceHeader.h>
#include "GridComponent.h"
#include "Utils.h"
#include "FrameQueue.h"


//=============================================================================
/*  The component that handles rendering the main MoPanning visulization.
*/
class GLVisualizer : public juce::OpenGLRenderer,
                      public juce::Component
{
public:
    //=========================================================================
    explicit GLVisualizer();
    ~GLVisualizer() override;

    //=========================================================================
    /*  Called when a new OpenGL context has been created.
        
        This is where we will create our textures, shaders, etc.
    */
    void newOpenGLContextCreated() override;

    /*  Called on the OpenGL thread when a new frame is to be rendered. 
    */
    void renderOpenGL() override;

    /*  Called when the current OpenGL context is about to close. 
    */
    void openGLContextClosing() override;

    //=========================================================================
    /*  Called when the component changes size.
    */
    void resized() override;

    //=========================================================================
    /*  Sets the pointer to the shared buffer holding results to be visualized.
    */
    void setResultsPointer(std::array<TrackSlot, Constants::maxTracks>* resultsPtr);

    /*  Sets the pointer to the shared queue for video writing output.
    */
    void setFrameQueuePointer(FrameQueue* frameQueuePtr);

    /*  Sets the dimension of the visualization (2D or 3D).
    */
    void setDimension(Dimension newDimension);

    /*  Sets the colour scheme for the given track. Needs Implementation!
    */
    void setTrackColourScheme(ColourScheme newColourScheme, int trackIndex);

    /*  Sets the frequency grid overlay as visible or not.
    */
    void setShowGrid(bool shouldShow);

    /*  Sets the minimum frequency, corresponding to the bottom of the screen.
    */
    void setMinFrequency(float newMinFrequency);

    /*  Sets the maximum frequency, corresponding to the top of the screen.
    */    
    void setMaxFrequency(float newMinFrequency);

    /*  Sets the speed at which the particles recede ("m/s").
    */
    void setRecedeSpeed(float newRecedeSpeed);

    /*  Sets the size of the particles relative to the default (1).
    */
    void setDotSize(float newDotSize);

    /*  Sets the distance at which particles disappear ("m")
    */
    void setFadeEndZ(float newFadeEndZ);

    //=========================================================================
    /*  Tells the component to start recording. Not yet implemented.
    */
    void startRecording();

    /*  Tells the component to stop recording. Not yet implemented.
    */
    void stopRecording();


private:
    //=========================================================================
    /*  This struct defines the layout of a single vertex in the VBO.
    */
    struct ParticleVertex
    {
        float frequency; // Band frequency in Hertz
        float amplitude; // 'Percieved' amplitude in range [0,1]
        float panIndex; // 'Percieved' lateralization in range [-1,1]
        float birthDistance; // Distance from camera when particle was created
        int trackIndex;  // Which track this particle belongs to
    };

    //========================================================================
    /*  Forward declarations of nested classes (see below).
    */
    struct VertexBuffer;
    struct Attributes;
    struct Uniforms;

    //=========================================================================
    /*  Compliles and links the shaders, and sets them active.
    */
    void createShaders();

    /*  Updates all uniform values on the GPU.
    
        This function should only be called from the GL thread!
    */
    void updateUniforms();

    /*  Creates a 2D colourmap (amplidude x track ID) and uploads it to GPU.
    */
    void buildTexture();

    //=========================================================================
    void renderFrame();
    void renderToScreen();
    void renderToCapture();

    //=========================================================================
    /*  Returns the colour corresponding to a colour scheme and amplitude value.
    */
    juce::Colour getColourForSchemeAndAmp(ColourScheme colourScheme, float amp);

    /*  Creates and returns a projection matrix for the given viewport size.
    */
    juce::Matrix3D<float> buildProjectionMatrix(float width, float height);

    //=========================================================================
    juce::OpenGLContext openGLContext;

    std::unique_ptr<OpenGLShaderProgram> mainShader;
    std::unique_ptr<VertexBuffer> vertexBuffer;
    std::unique_ptr<Attributes> attributes;
    std::unique_ptr<Uniforms> uniforms;

    std::array<TrackSlot, Constants::maxTracks>* results;
    FrameQueue* frameQueue;

    float startTime;
    float lastFrameTime;

    float globalDistance = 0.0f; // Total (positive) distance traveled

    juce::Vector3D<float> cameraPosition { 0.0f, 0.0f, -2.0f };
    juce::Matrix3D<float> view = juce::Matrix3D<float>::fromTranslation(cameraPosition);
    juce::Matrix3D<float> displayProj; // Projection matrix for the display window
    juce::Matrix3D<float> captureProj; // Projection matrix for the video capture

    juce::OpenGLTexture colourMapTexture;
    std::atomic<bool> textureNeedsRebuild { true };

    juce::OpenGLFrameBuffer captureFBO;
    static constexpr int captureW = 1280, captureH = 720;
    bool recording;
    std::vector<uint8_t> capturePixels;

    std::unique_ptr<GridComponent> grid;

    //=========================================================================
    /* Parameters */

    Dimension dimension;
    std::array<std::atomic<ColourScheme>, Constants::maxTracks> trackColourSchemes;
    
    int numTracks; // Number of tracks in the results vector
    float minFrequency; // Minimum frequency to display (Hz)
    float maxFrequency = 20000.0f; // Maximum frequency to display (Hz)
    float recedeSpeed; // Speed that objects recede
    float dotSize; // Radius of the dots
    float fadeEndZ; // (Positive) distance at which points are fully faded (m)

    static constexpr float nearZ = 0.1f; // Distance to the start of clip space (m)
    static constexpr float farZ = 100.f; // Distance to the end of clip space (m)
    static constexpr float fov = 45.f; // Vertical field of view (degrees)
    static constexpr int maxParticles = 200000;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLVisualizer)
};


//=============================================================================
/*  This struct manages a vertex buffer object (VBO).
*/
struct GLVisualizer::VertexBuffer
{
    //=========================================================================
    explicit VertexBuffer();
    ~VertexBuffer();

    //=========================================================================
    /*  Updates the particle array with new data from the results buffer.
    */
    void updateParticles(std::array<TrackSlot, Constants::maxTracks>* results, 
                         float globalDistance, 
                         float fadeEndZ);

    /*  Draws all active particles.
    */
    void draw(Attributes& glAttributes);

    GLuint vaoID;
    GLuint vboID;

    std::array<ParticleVertex, maxParticles> particles;

    int oldestIndex = 0;
    int numActiveVertices = 0;
    float lastUpdateTime = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VertexBuffer)
};


//=============================================================================
/*  This struct manages the attributes that the shaders use.
*/
struct GLVisualizer::Attributes
{
    explicit Attributes(OpenGLShaderProgram& shaderProgram);

    /*  Enables each of the attributes held in the struct.
    */
    void enable();

    /*  Disables each of the attributes held in the struct.
    */
    void disable();

    std::unique_ptr<OpenGLShaderProgram::Attribute> frequency; 
    std::unique_ptr<OpenGLShaderProgram::Attribute> amplitude;
    std::unique_ptr<OpenGLShaderProgram::Attribute> panIndex;
    std::unique_ptr<OpenGLShaderProgram::Attribute> birthDistance;
    std::unique_ptr<OpenGLShaderProgram::Attribute> trackIndex;

private:
    /*  Safely creates a new juce::OpenGLShaderProgram::Attribute
    */
    static OpenGLShaderProgram::Attribute* createAttribute(OpenGLShaderProgram& shader,
                                                           const char* attributeName);
};


//=============================================================================
/* This struct manages the uniform values that the shaders use.
*/
struct GLVisualizer::Uniforms
{
    explicit Uniforms(OpenGLShaderProgram& shaderProgram);

    std::unique_ptr<OpenGLShaderProgram::Uniform> colourMap;
    std::unique_ptr<OpenGLShaderProgram::Uniform> projectionMatrix;
    std::unique_ptr<OpenGLShaderProgram::Uniform> viewMatrix;
    std::unique_ptr<OpenGLShaderProgram::Uniform> windowSize; 
    std::unique_ptr<OpenGLShaderProgram::Uniform> minFrequency;
    std::unique_ptr<OpenGLShaderProgram::Uniform> maxFrequency;
    std::unique_ptr<OpenGLShaderProgram::Uniform> globalDistance;
    std::unique_ptr<OpenGLShaderProgram::Uniform> fadeEndZ; 
    std::unique_ptr<OpenGLShaderProgram::Uniform> dotSize;

private:
    /*  Safely creates a new juce::OpenGLShaderProgram::Uniform
    */
    static OpenGLShaderProgram::Uniform* createUniform(OpenGLShaderProgram& shader,
                                                        const char* uniformName);
};