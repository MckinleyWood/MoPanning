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
#include "Utils.h"

//=============================================================================
/*  A component for rendering a dot cloud with OpenGL.
*/
class GLVisualizer2 : public juce::OpenGLRenderer,
                      public juce::Component
{
public:
    //=========================================================================
    explicit GLVisualizer2();
    ~GLVisualizer2() override;

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
    void createGridImageFromComponent(juce::Component* gridComp);

    void setResultsPointer(std::array<TrackSlot, 8>* resultsPtr);
    
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
    /*  This struct manages a vertex buffer object (VBO).
    */
    struct VertexBuffer;

    /*  This struct manages the attributes that the shaders use.
    */
    struct Attributes;

    /* This struct manages the uniform values that the shaders use.
    */
    struct Uniforms;

    //=========================================================================
    /*  Initializes the shaders...
    */
    void createShaders();

    /*  Updates all uniform values on the GPU.
    
        This function should only be called on the GL thread!
    */
    void updateUniforms();
    
    /*  Creates a projection matrix for the given viewport size.
    */
    juce::Matrix3D<float> buildProjectionMatrix(float width, float height);

    //=========================================================================
    juce::OpenGLContext openGLContext;

    std::unique_ptr<OpenGLShaderProgram> mainShader;
    std::unique_ptr<VertexBuffer> vertexBuffer;
    std::unique_ptr<Attributes> attributes;
    std::unique_ptr<Uniforms> uniforms;

    std::array<TrackSlot, 8>* results; // Pointer to the shared results buffer

    float startTime;
    float lastFrameTime;

    float globalDistance = 0.0f; // Total (positive) distance traveled

    juce::Vector3D<float> cameraPosition { 0.0f, 0.0f, -2.0f };
    juce::Matrix3D<float> view = juce::Matrix3D<float>::fromTranslation(cameraPosition);
    juce::Matrix3D<float> displayProj; // Projection matrix for the display window
    juce::Matrix3D<float> captureProj; // Projection matrix for the video capture

    //=========================================================================
    /* Parameters */

    Dimension dimension;
    std::vector<ColourScheme> trackColourSchemes;
    std::vector<GLuint> trackColourTextures;
    
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLVisualizer2)
};

//=============================================================================
struct GLVisualizer2::VertexBuffer
{
    explicit VertexBuffer();
    ~VertexBuffer();

    void updateParticles(std::array<TrackSlot, 8>* results, float globalDistance, float fadeEndZ);
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
struct GLVisualizer2::Attributes
{
    explicit Attributes(OpenGLShaderProgram& shaderProgram)
    {
        frequency.reset(createAttribute(shaderProgram, "frequency"));
        amplitude.reset(createAttribute(shaderProgram, "amplitude"));
        panIndex.reset(createAttribute(shaderProgram, "panIndex"));
        birthDistance.reset(createAttribute(shaderProgram, "birthDistance"));
        trackIndex.reset(createAttribute(shaderProgram, "trackIndex"));
    }

    void enable()
    {
        using namespace ::juce::gl;

        if (frequency.get() != nullptr)
        {
            glVertexAttribPointer(frequency->attributeID, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), nullptr);
            glEnableVertexAttribArray(frequency->attributeID);
        }

        if (amplitude.get() != nullptr)
        {
            glVertexAttribPointer(amplitude->attributeID, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (GLvoid*)(sizeof(float)));
            glEnableVertexAttribArray(amplitude->attributeID);
        }

        if (panIndex.get() != nullptr)
        {
            glVertexAttribPointer(panIndex->attributeID, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (GLvoid*)(sizeof(float) * 2));
            glEnableVertexAttribArray(panIndex->attributeID);
        }

        if (birthDistance.get() != nullptr)
        {
            glVertexAttribPointer(birthDistance->attributeID, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (GLvoid*)(sizeof(float) * 3));
            glEnableVertexAttribArray(birthDistance->attributeID);
        }

        if (trackIndex.get() != nullptr)
        {
            glVertexAttribIPointer(trackIndex->attributeID, 1, GL_INT, sizeof(ParticleVertex), (GLvoid*)(sizeof(float) * 4));
            glEnableVertexAttribArray(trackIndex->attributeID);
        }
    }

    void disable()
    {
        using namespace ::juce::gl;

        if (frequency != nullptr)
            glDisableVertexAttribArray(frequency->attributeID);

        if (amplitude != nullptr)
            glDisableVertexAttribArray(amplitude->attributeID);

        if (panIndex != nullptr)
            glDisableVertexAttribArray(panIndex->attributeID);

        if (birthDistance != nullptr)
            glDisableVertexAttribArray(birthDistance->attributeID);

        if (trackIndex != nullptr)
            glDisableVertexAttribArray(trackIndex->attributeID);
    }

    std::unique_ptr<OpenGLShaderProgram::Attribute> frequency; 
    std::unique_ptr<OpenGLShaderProgram::Attribute> amplitude;
    std::unique_ptr<OpenGLShaderProgram::Attribute> panIndex;
    std::unique_ptr<OpenGLShaderProgram::Attribute> birthDistance;
    std::unique_ptr<OpenGLShaderProgram::Attribute> trackIndex;

private:
    static OpenGLShaderProgram::Attribute* createAttribute(OpenGLShaderProgram& shader,
                                                           const char* attributeName)
    {
        using namespace ::juce::gl;

        if (glGetAttribLocation(shader.getProgramID(), attributeName) < 0)
            return nullptr;

        return new OpenGLShaderProgram::Attribute(shader, attributeName);
    }
};

//=============================================================================
struct GLVisualizer2::Uniforms
{
    explicit Uniforms(OpenGLShaderProgram& shaderProgram)
    {
        projectionMatrix.reset(createUniform(shaderProgram, "uProjectionMatrix"));
        viewMatrix.reset(createUniform(shaderProgram, "uViewMatrix"));
        windowSize.reset(createUniform(shaderProgram, "uWindowSize"));
        minFrequency.reset(createUniform(shaderProgram, "uMinFrequency"));
        maxFrequency.reset(createUniform(shaderProgram, "uMaxFrequency"));
        globalDistance.reset(createUniform(shaderProgram, "uGlobalDistance"));
        fadeEndZ.reset(createUniform(shaderProgram, "uFadeEndZ"));
        dotSize.reset(createUniform(shaderProgram, "uDotSize"));
    }

    std::unique_ptr<OpenGLShaderProgram::Uniform> projectionMatrix;
    std::unique_ptr<OpenGLShaderProgram::Uniform> viewMatrix;
    std::unique_ptr<OpenGLShaderProgram::Uniform> windowSize; 
    std::unique_ptr<OpenGLShaderProgram::Uniform> minFrequency;
    std::unique_ptr<OpenGLShaderProgram::Uniform> maxFrequency;
    std::unique_ptr<OpenGLShaderProgram::Uniform> globalDistance;
    std::unique_ptr<OpenGLShaderProgram::Uniform> fadeEndZ; 
    std::unique_ptr<OpenGLShaderProgram::Uniform> dotSize;

private:
    static OpenGLShaderProgram::Uniform* createUniform(OpenGLShaderProgram& shaderProgram,
                                                        const char* uniformName)
    {
        using namespace ::juce::gl;

        if (glGetUniformLocation(shaderProgram.getProgramID(), uniformName) < 0)
            return nullptr;

        return new OpenGLShaderProgram::Uniform(shaderProgram, uniformName);
    }
};