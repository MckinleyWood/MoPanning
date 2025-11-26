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

#include "GLVisualizer2.h"

//=============================================================================
GLVisualizer2::GLVisualizer2()
{
    auto glVersion = juce::OpenGLContext::OpenGLVersion::openGL3_2;
    openGLContext.setOpenGLVersionRequired(glVersion);
    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);
    openGLContext.setContinuousRepainting(true);

    startTime = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
    lastFrameTime = startTime;
}

GLVisualizer2::~GLVisualizer2()
{
    openGLContext.detach();
}

//=============================================================================
void GLVisualizer2::newOpenGLContextCreated() 
{
    using namespace juce::gl;
    
    // Initialize GL resources
    createShaders();
    vertexBuffer = std::make_unique<VertexBuffer>();
    attributes = std::make_unique<Attributes>(*mainShader);
    uniforms = std::make_unique<Uniforms>(*mainShader);

    glEnable(GL_PROGRAM_POINT_SIZE);
}

void GLVisualizer2::renderOpenGL() 
{
    using namespace juce::gl;

    // Update the global distance/time counters
    float t = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001 - startTime);
    float dt = t - lastFrameTime; // Time since last frame in seconds
    float dz = dt * recedeSpeed; // Distance receded since last frame (m)

    globalDistance += dz;
    lastFrameTime = t;

    // Ensure we have the latest parameter values for our uniforms
    updateUniforms();

    // Rebuild the colourmap 
    if (textureNeedsRebuild.load())
        buildTexture();

    // Bind the colourmap texture
    glActiveTexture(GL_TEXTURE0);
    colourMapTexture.bind();

    // Add the new particles to the VBO
    vertexBuffer->updateParticles(results, globalDistance, fadeEndZ);

    // Draw
    juce::OpenGLHelpers::clear(juce::Colours::black);
    vertexBuffer->draw(*attributes);

    colourMapTexture.unbind();
}

void GLVisualizer2::openGLContextClosing() 
{
    // Clean up GL resources
    mainShader.reset();
    vertexBuffer.reset();
    attributes.reset();
    uniforms.reset();
    colourMapTexture.release();
}

//=============================================================================
void GLVisualizer2::resized()
{
    displayProj = buildProjectionMatrix((float)getWidth(), (float)getHeight());
}

//=============================================================================
void GLVisualizer2::setResultsPointer(std::array<TrackSlot, Constants::maxTracks>* resultsPtr)
{
    results = resultsPtr;
}

void GLVisualizer2::setDimension(Dimension newDimension)
{
    dimension = newDimension;
    resized(); // Force a resize to update the projection matrix
}

void GLVisualizer2::setTrackColourScheme(ColourScheme newColourScheme, int trackIndex)
{
    // Check bounds
    if (trackIndex < 0 || trackIndex >= trackColourSchemes.size())
        return;

    // Update track colour scheme and set flag
    trackColourSchemes[trackIndex].store(newColourScheme);
    textureNeedsRebuild.store(true);
}

void GLVisualizer2::setShowGrid(bool shouldShow)
{
    // showGrid = shouldShow;
}

void GLVisualizer2::setMinFrequency(float newMinFrequency)
{
    minFrequency = newMinFrequency;
}

void GLVisualizer2::setMaxFrequency(float newMaxFrequency)
{
    maxFrequency = newMaxFrequency;
}

void GLVisualizer2::setRecedeSpeed(float newRecedeSpeed)
{
    recedeSpeed = newRecedeSpeed;
}

void GLVisualizer2::setDotSize(float newDotSize)
{
    dotSize = newDotSize;
}

void GLVisualizer2::setFadeEndZ(float newFadeEndZ)
{
    fadeEndZ = newFadeEndZ;
}

//=============================================================================
void GLVisualizer2::startRecording()
{
    // recording = true;
    // resized(); // Update projection matrix
}

void GLVisualizer2::stopRecording()
{
    // recording = false;
    // resized(); // Update projection matrix
}

//=============================================================================
void GLVisualizer2::createShaders()
{
    juce::String vertexShaderCode = 
    R"(
        #version 150

        in float frequency;
        in float amplitude;
        in float panIndex;
        in float birthDistance; // Distance traveled when particle was created
        in int trackIndex;

        uniform sampler2D uColourMap;

        uniform mat4 uProjectionMatrix;
        uniform mat4 uViewMatrix;

        uniform vec2 uWindowSize;
        uniform float uMinFrequency;
        uniform float uMaxFrequency;
        uniform float uGlobalDistance; // Distance traveled at current frame
        uniform float uFadeEndZ;
        uniform float uDotSize;

        out vec4 colour;

        void main()
        {
            // Calculate world position and size based on attributes
            float logMin = log(uMinFrequency);
            float logMax = log(uMaxFrequency);

            float aspect = uWindowSize.x / uWindowSize.y;

            float x = panIndex * aspect;
            float y = (log(frequency) - logMin) / (logMax - logMin) * 2.0 - 1.0;
            float z = (uGlobalDistance - birthDistance) * -1.0;
            vec4 worldPosition = vec4(x, y, z, 1.0);

            gl_Position = uProjectionMatrix * uViewMatrix * worldPosition;
            gl_PointSize = (0.5 + amplitude) * uDotSize * uWindowSize.y * 0.008;
            // gl_PointSize = frequency / 1000.0;

            // Calculate colour and alpha
            float depth = -z / uFadeEndZ;
            float alpha = (0.5 + amplitude * 0.5) * (1.0 - depth);
            
            // Look up color from the texture
            float u = amplitude;
            float v = 1.0 - (float(trackIndex) + 0.5) / 8.0;
            vec3 rgb = texture(uColourMap, vec2(u, v)).rgb;

            colour = vec4(rgb, alpha);
        }
    )";
    
    juce::String fragmentShaderCode = 
    R"(
        #version 150

        in vec4 colour;
        out vec4 frag;

        void main()
        {
            // Remap coordinates to [-1, +1]
            vec2 p = gl_PointCoord * 2.0 - 1.0;

            // Discard fragments outside the circle
            if (dot(p, p) > 1.0) discard;

            // Calculate alpha with soft edge
            // float r2 = dot(p, p);
            // float soft = 1.0 - smoothstep(0.9, 1, r2);

            float fadeFactor = colour.a;

            vec3 rgb = colour.rgb * fadeFactor; // PREMULTIPLIED
            // vec3 rgb = colour.rgb; // NOT

            frag = vec4(rgb, 1.0);
        }
    )";

    mainShader = std::make_unique<OpenGLShaderProgram>(openGLContext);
    if (mainShader->addVertexShader(vertexShaderCode) == false
     || mainShader->addFragmentShader(fragmentShaderCode) == false
     || mainShader->link() == false)
    {
        DBG("Shader compilation/linking failed: " + mainShader->getLastError());
        return;
    }

    mainShader->use();
}

void GLVisualizer2::updateUniforms()
{
    jassert(uniforms != nullptr 
         && uniforms->colourMap != nullptr
         && uniforms->projectionMatrix != nullptr
         && uniforms->viewMatrix != nullptr
         && uniforms->windowSize != nullptr
         && uniforms->minFrequency != nullptr
         && uniforms->maxFrequency != nullptr
         && uniforms->globalDistance != nullptr
         && uniforms->fadeEndZ != nullptr
         && uniforms->dotSize != nullptr
    );
    
    uniforms->colourMap->set(0);
    uniforms->projectionMatrix->setMatrix4(displayProj.mat, 1, false);
    uniforms->viewMatrix->setMatrix4(view.mat, 1, false);
    uniforms->windowSize->set((float)getWidth(), (float)getHeight());
    uniforms->minFrequency->set(minFrequency);
    uniforms->maxFrequency->set(maxFrequency);
    uniforms->globalDistance->set(globalDistance);
    uniforms->fadeEndZ->set(fadeEndZ);
    uniforms->dotSize->set(dotSize);
}

void GLVisualizer2::buildTexture()
{
    juce::Image colourMapImage(juce::Image::RGB, 256, Constants::maxTracks, true);

    for (int y = 0; y < Constants::maxTracks; ++y)
    {
        ColourScheme colourScheme = trackColourSchemes[y].load();

        for (int x = 0; x < 256; ++x)
        {
            juce::Colour colour = getColourForSchemeAndAmp(colourScheme, x / 255.0f);
            colourMapImage.setPixelAt(x, y, colour);
        }
    }

    // Upload the new texture to the GPU
    colourMapTexture.loadImage(colourMapImage);

    textureNeedsRebuild.store(false);
}

juce::Colour GLVisualizer2::getColourForSchemeAndAmp(ColourScheme colourScheme, float amp)
{
    float hue;
    float val;

    switch (colourScheme)
    {
        case greyscale:
            return juce::Colour::fromFloatRGBA(amp, amp, amp, 1.0f);

        case rainbow:
            return juce::Colour::fromHSV(amp, 1.0f, 1.0f, 1.0f);

        case red:
            hue = 0.0f;
            val = juce::jmap(amp, 0.0f, 1.0f, 0.6f, 1.0f);  // high amp = brighter
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        case orange:
            hue = 0.06f;
            val = juce::jmap(amp, 0.0f, 1.0f, 0.9f, 1.0f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        case yellow:
            hue = 0.13f;
            val = juce::jmap(amp, 0.0f, 1.0f, 0.95f, 1.0f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        case lightGreen:
            hue = 0.25f;
            val = juce::jmap(amp, 0.0f, 1.0f, 0.6f, 1.0f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        case darkGreen:
            hue = 0.35f;
            val = juce::jmap(amp, 0.0f, 1.0f, 0.2f, 0.6f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        case lightBlue:
            hue = 0.52;
            val = juce::jmap(amp, 0.0f, 1.0f, 0.8f, 1.0f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        case darkBlue:
            hue = 0.63;
            val = juce::jmap(amp, 0.0f, 1.0f, 0.8f, 1.0f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);
        
        case purple:
            hue = 0.8f;
            val = juce::jmap(amp, 0.0f, 1.0f, 0.8f, 1.0f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        case pink:
            hue = 0.9f;
            val = juce::jmap(amp, 0.0f, 1.0f, 0.8f, 1.0f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        case warm:
            // Red → oragne → pale gold
            hue = juce::jmap(amp, 0.0f, 1.0f, 0.00f, 0.13f);
            val = juce::jmap(amp, 0.0f, 1.0f, 0.8f, 1.0f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        case cool:
            // Purple → blue → green
            hue = juce::jmap(amp, 0.0f, 1.0f, 0.85f, 0.38f);
            val = juce::jmap(amp, 0.0f, 1.0f, 0.8f, 1.0f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        case slider:
            float hueMax;
            float hueMin;
            hue = juce::jmap(amp, 0.0f, 1.0f, hueMin, hueMax);
            val = juce::jmap(amp, 0.0f, 1.0f, 0.8f, 1.0f);
            return juce::Colour::fromHSV(hue, 1.0f, val, 1.0f);

        default:
            jassertfalse;
    }
}

juce::Matrix3D<float> GLVisualizer2::buildProjectionMatrix(float width, float height)
{
    const float aspect = width / height;
    const float fovRadians = fov * juce::MathConstants<float>::pi / 180;
    const float l = -aspect, r = +aspect;
    const float b = -1.0f, t = +1.0f;
    const float n = nearZ, f = farZ;

    juce::Matrix3D<float> proj;

    switch (dimension)
    {
    case twoD:
        // Orthographic projection for 2D
        proj = juce::Matrix3D<float>(
            2.0f / (r - l), 0, 0, 0,
            0, 2.0f / (t - b), 0, 0,
            0, 0, -2.0f / (f - n), 0,
           -(r + l) / (r - l), -(t + b) / (t-b), -(f + n) / (f - n), 1.0f);
        break;
    
    case threeD:
        // Perspective projection for 3D
        proj = juce::Matrix3D<float>::fromFrustum(
              -nearZ * std::tan(fovRadians * 0.5f) * aspect,   // left
               nearZ * std::tan(fovRadians * 0.5f) * aspect,   // right
              -nearZ * std::tan(fovRadians * 0.5f),            // bottom
               nearZ * std::tan(fovRadians * 0.5f),            // top
               nearZ, farZ);
        break;
    
    default:
        jassertfalse; // Unknown dimension
    }

    return proj;
}


//=============================================================================
GLVisualizer2::VertexBuffer::VertexBuffer()
{
    using namespace juce::gl;

    glGenVertexArrays(1, &vaoID);
    glGenBuffers(1, &vboID);
}

GLVisualizer2::VertexBuffer::~VertexBuffer()
{
    using namespace juce::gl;

    glDeleteBuffers(1, &vboID);
    glDeleteVertexArrays(1, &vaoID);
}

//=============================================================================
void GLVisualizer2::VertexBuffer::updateParticles(
            std::array<TrackSlot, Constants::maxTracks>* results, 
            float globalDistance, 
            float fadeEndZ)
{
    // Remove particles that have faded out
    while (numActiveVertices > 0)
    {
        ParticleVertex& oldestParticle = particles[oldestIndex];
        float distanceTravelled = (globalDistance - oldestParticle.birthDistance);

        if (distanceTravelled < fadeEndZ)
            break; // Oldest particle is still alive
        
        // Otherwise, discard it
        oldestIndex = (oldestIndex + 1) % maxParticles;
        --numActiveVertices;
    }

    // Add new particles from the latest analysis results
    for (int i = 0; i < results->size(); ++i)
    {
        const auto& slot = (*results)[i];
        int activeIndex = slot.activeIndex.load(std::memory_order_acquire);
        const auto& result = slot.buffers[activeIndex];

        if (result.empty()) continue;

        for (const FrequencyBand& band : result)
        {
            int index = (oldestIndex + numActiveVertices) % maxParticles;
            ParticleVertex& newParticle = particles[index];
            newParticle.frequency = band.frequency;
            newParticle.amplitude = band.amplitude;
            newParticle.panIndex = band.panIndex;
            newParticle.birthDistance = globalDistance;
            newParticle.trackIndex = band.trackIndex;

            ++numActiveVertices;
        }

        if (numActiveVertices > maxParticles)
        {
            // We have overwritten some vertices
            int difference = numActiveVertices - maxParticles;
            oldestIndex += difference;
            numActiveVertices = maxParticles;
        }
    }
}

void GLVisualizer2::VertexBuffer::draw(Attributes& glAttributes)
{
    using namespace juce::gl;

    // Bind vao and vbo and enable the attributes
    glBindVertexArray(vaoID);
    glBindBuffer(GL_ARRAY_BUFFER, vboID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ParticleVertex) * maxParticles, nullptr, GL_DYNAMIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ParticleVertex) * maxParticles, particles.data(), GL_DYNAMIC_DRAW);
    
    glAttributes.enable();

    // Draw the particles from the oldest to the end of the buffer
    glDrawArrays(GL_POINTS, oldestIndex, std::min(numActiveVertices, maxParticles - oldestIndex));

    // If the buffer has wrapped around, draw the remaining particles 
    glDrawArrays(GL_POINTS, 0, std::max(0, numActiveVertices - (maxParticles - oldestIndex)));

    glAttributes.disable();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}


//=============================================================================
GLVisualizer2::Attributes::Attributes(OpenGLShaderProgram& shaderProgram)
{
    frequency.reset(createAttribute(shaderProgram, "frequency"));
    amplitude.reset(createAttribute(shaderProgram, "amplitude"));
    panIndex.reset(createAttribute(shaderProgram, "panIndex"));
    birthDistance.reset(createAttribute(shaderProgram, "birthDistance"));
    trackIndex.reset(createAttribute(shaderProgram, "trackIndex"));
}

void GLVisualizer2::Attributes::enable()
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

void GLVisualizer2::Attributes::disable()
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

OpenGLShaderProgram::Attribute* GLVisualizer2::Attributes::createAttribute(
                            OpenGLShaderProgram& shader, const char* attributeName)
{
    using namespace ::juce::gl;

    if (glGetAttribLocation(shader.getProgramID(), attributeName) < 0)
        return nullptr;

    return new OpenGLShaderProgram::Attribute(shader, attributeName);
}


//=============================================================================
GLVisualizer2::Uniforms::Uniforms(OpenGLShaderProgram& shaderProgram)
{
    colourMap.reset(createUniform(shaderProgram, "uColourMap"));
    projectionMatrix.reset(createUniform(shaderProgram, "uProjectionMatrix"));
    viewMatrix.reset(createUniform(shaderProgram, "uViewMatrix"));
    windowSize.reset(createUniform(shaderProgram, "uWindowSize"));
    minFrequency.reset(createUniform(shaderProgram, "uMinFrequency"));
    maxFrequency.reset(createUniform(shaderProgram, "uMaxFrequency"));
    globalDistance.reset(createUniform(shaderProgram, "uGlobalDistance"));
    fadeEndZ.reset(createUniform(shaderProgram, "uFadeEndZ"));
    dotSize.reset(createUniform(shaderProgram, "uDotSize"));
}

OpenGLShaderProgram::Uniform* GLVisualizer2::Uniforms::createUniform(
                    OpenGLShaderProgram& shader, const char* uniformName)
{
    using namespace ::juce::gl;

    if (glGetUniformLocation(shader.getProgramID(), uniformName) < 0)
        return nullptr;

    return new OpenGLShaderProgram::Uniform(shader, uniformName);
}