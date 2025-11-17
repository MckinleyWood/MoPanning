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

#include "GLVisualizer.h"
#include "MainController.h"
#include "GridComponent.h"

//=============================================================================
GLVisualizer::GLVisualizer(MainController& c) : controller(c) 
{
    juce::ignoreUnused(c);

    auto glVersion = juce::OpenGLContext::OpenGLVersion::openGL3_2;
    openGLContext.setOpenGLVersionRequired(glVersion);
    openGLContext.setContinuousRepainting(true);

    setInterceptsMouseClicks(false, false); // don't block mouse or focus
    setWantsKeyboardFocus(false);           // don't take keyboard focus

    startTime = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
    lastFrameTime = startTime;
}

GLVisualizer::~GLVisualizer()
{
    openGLContext.setContinuousRepainting(false);
    openGLContext.detach();
    shutdownOpenGL();
}

//=============================================================================
/*  This function is called when the OpenGL context is created. It is
    where we initialize all of our GL resources, including the shaders
    for the main dot cloud and the grid overlay, the colourmap texture,
    and the VAOS and VBOs.
*/
void GLVisualizer::initialise() 
{
    using namespace juce::gl;
    auto& ext = openGLContext.extensions;

    capturePixels.resize(captureW * captureH * 3);
    flippedPixels.resize(captureW * captureH * 3);
    captureProj = buildProjectionMatrix(captureW, captureH);
    view = juce::Matrix3D<float>::fromTranslation(cameraPosition);

    // GLSL vertex shader initialization code for the dot cloud
    static const char* mainVertSrc = R"(#version 150
        in vec4 instanceData;

        uniform mat4 uProjection;
        uniform mat4 uView;
        uniform sampler1D uColourMap;
        uniform float uWidth;
        uniform float uHeight;
        uniform float uFadeEndZ;
        uniform float uDotSize;

        out vec4 vColour;
        
        void main()
        {
            float amp = instanceData.w;

            // Depth factor for fading effect
            float depth = -instanceData.z / uFadeEndZ;
            float alpha = (0.5 + amp * 0.5) * (1.0 - depth);
            
            // Look up color from the texture
            vec3 rgb = texture(uColourMap, amp).rgb;

            // Set the color with alpha
            vColour = vec4(rgb, alpha);

            // Build world position
            float aspect = uWidth / uHeight;
            float x = instanceData.x * aspect;
            vec4 worldPos = vec4(x, instanceData.yz, 1.0);

            // Compute clip-space coordinate
            gl_Position = uProjection * uView * worldPos;

            // Size in pixels
            gl_PointSize = (0.5 + amp) * uDotSize * uHeight * 0.008;
        }
    )";

    // GLSL fragment shader initialization code for the dot cloud
    static const char* mainFragSrc = R"(#version 150
        in vec4 vColour;
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

            float fadeFactor = vColour.a;

            vec3 rgb = vColour.rgb * fadeFactor; // PREMULTIPLIED
            // vec3 rgb = vColour.rgb; // NOT

            frag = vec4(rgb, 1.0);
        }
    )";
    
    // Compile and link the main shaders
    mainShader.reset(new juce::OpenGLShaderProgram(openGLContext));
    mainShader->addVertexShader(mainVertSrc);
    mainShader->addFragmentShader(mainFragSrc);

    ext.glBindAttribLocation(mainShader->getProgramID(), 0, "instanceData");

    bool shaderLinked = mainShader->link();
    jassert(shaderLinked);
    juce::ignoreUnused(shaderLinked);

    buildTexture();
    
    // Generate and bind the main vertex-array object
    ext.glGenVertexArrays(1, &mainVAO);
    ext.glBindVertexArray(mainVAO);

    // Create instance buffer
    ext.glGenBuffers(1, &instanceVBO);

    // Bind the instanceVBO to set up the attribute pointer for instancing
    ext.glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    ext.glBufferData(GL_ARRAY_BUFFER,
                     maxParticles * sizeof(InstanceData),
                     nullptr,
                     GL_DYNAMIC_DRAW);
    ext.glEnableVertexAttribArray(0);
    ext.glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 
                              sizeof(InstanceData), nullptr);
    glVertexAttribDivisor(0, 1); // This attribute advances once per instance
    
    // Unbind VAO to avoid accidental state leakage
    ext.glBindVertexArray(0);

    // Shader code for rendering the grid
    static const char* gridVertSrc = R"(#version 150
        in vec2 aPos;
        in vec2 aUV;
        out vec2 vUV;
        void main() 
        {
            vUV = aUV;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    static const char* gridFragSrc = R"(#version 150
        uniform sampler2D uTex;
        in vec2 vUV;
        out vec4 fragColor;
        void main() 
        {
            fragColor = texture(uTex, vUV);
        }
    )";

    // Complile and link the grid shaders
    gridShader.reset(new juce::OpenGLShaderProgram(openGLContext));
    gridShader->addVertexShader(gridVertSrc);
    gridShader->addFragmentShader(gridFragSrc);

    shaderLinked = gridShader->link();
    jassert(shaderLinked);

    // Create VAO/VBO
    ext.glGenVertexArrays(1, &gridVAO);
    ext.glBindVertexArray(gridVAO);

    ext.glGenBuffers(1, &gridVBO);
    ext.glBindBuffer(GL_ARRAY_BUFFER, gridVBO);

    // Interleaved pos(x,y), uv(u,v)
    static const float quadData[] = {
        -1.f, -1.f,  0.f, 1.f,
        1.f, -1.f,  1.f, 1.f,
        -1.f,  1.f,  0.f, 0.f,
        1.f,  1.f,  1.f, 0.f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(quadData), quadData, GL_STATIC_DRAW);

    // Attribute locations
    GLint posLoc = ext.glGetAttribLocation(gridShader->getProgramID(), "aPos");
    GLint uvLoc  = ext.glGetAttribLocation(gridShader->getProgramID(), "aUV");
    glEnableVertexAttribArray((GLuint)posLoc);
    glEnableVertexAttribArray((GLuint)uvLoc);
    glVertexAttribPointer((GLuint)posLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glVertexAttribPointer((GLuint)uvLoc,  2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    ext.glBindVertexArray(0);
    ext.glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLVisualizer::shutdown()
{
    auto& ext = openGLContext.extensions;
    
    // Delete VAO and VBOs
    if (mainVAO != 0) 
    { 
        ext.glDeleteVertexArrays(1, &mainVAO); 
        mainVAO = 0; 
    }
    if (instanceVBO != 0) 
    {
        ext.glDeleteBuffers(1, &instanceVBO);
        instanceVBO = 0;
    }
    if (gridVBO != 0) 
    { 
        ext.glDeleteBuffers(1, &gridVBO); 
        gridVBO = 0; 
    }
    if (gridVAO) 
    { 
        ext.glDeleteVertexArrays(1, &gridVAO); 
        gridVAO = 0; 
    }

    // Delete the grid texture
    gridGLTex.release();

    mainShader.reset();
    gridShader.reset();
}

void GLVisualizer::render()
{
    using namespace juce::gl;
    auto& ext = openGLContext.extensions;

    // Resize per-track containers if needed
    if ((int)trackColourTextures.size() <= numTracks)
    {
        trackColourTextures.resize (numTracks + 1, 0);
        trackColourSchemes .resize (numTracks + 1, rainbow);
        newTextureRequested = true;          // forces buildTexture() next frame
        // DBG ("Resized track containers to " << numTracks + 1);
    }

    // Check if we need to rebuild the colourmap texture
    buildTexture();

    // Update the particle vector
    updateParticles();

    if (recording)
    {
        // Ensure FBO is the right size
        updateFBOSize();

        // Bind the capture FBO
        captureFBO.makeCurrentAndClear();
        glViewport(0, 0, captureW, captureH);
        
        const float scale = openGLContext.getRenderingScale();
        
        // Render to the capture FBO
        drawParticles(captureW, captureH, captureProj);
    
        if (showGrid)
            drawGrid();

        // Read pixels from the FBO to CPU memory
        glReadPixels(0, 0, captureW, captureH, 
                     GL_RGB, GL_UNSIGNED_BYTE, 
                     capturePixels.data());

        // Flip the image vertically
        const int rowBytes = captureW * 3;
        for (int y = 0; y < captureH; ++y)
        {
            const uint8_t* src = capturePixels.data() + (size_t)(captureH - 1 - y) * rowBytes;
            uint8_t* dst = flippedPixels.data() + (size_t)y * rowBytes;
            std::memcpy(dst, src, (size_t)rowBytes);
        }

        // Enque the frame to the VideoWriter
        controller.giveFrameToVideoWriter(flippedPixels.data(), (int)flippedPixels.size());

        // Set destination viewport back to the window
        ext.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        const int winW = int(std::round(getWidth()  * scale));
        const int winH = int(std::round(getHeight() * scale));
        glViewport(0, 0, winW, winH);
    }

    // Render to the default framebuffer (the window)
    juce::OpenGLHelpers::clear(juce::Colours::black);
    glClear(GL_DEPTH_BUFFER_BIT);

    drawParticles(getWidth(), getHeight(), projection);

    if (showGrid)
        drawGrid();

    // Check for OpenGL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        DBG("OpenGL error: " << juce::String::toHexString((int)err));
}

void GLVisualizer::resized() 
{
    projection = buildProjectionMatrix(getWidth(), getHeight());
}

void GLVisualizer::createGridImageFromComponent(GridComponent* gridComp)
{
    if (getWidth() <= 0 || getHeight() <= 0 || gridComp == nullptr)
        return;

    // Render the grid component into a JUCE Image on the message thread
    juce::Image img(juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g(img);
    gridComp->paint(g);

    // Copy into member variable
    gridImage = img;

    // Mark the texture as dirty so it will be uploaded next render call
    gridTextureDirty.store(true);
}

//=============================================================================
void GLVisualizer::setResultsPointer(std::array<TrackSlot, 8>* resultsPtr)
{
    results = resultsPtr;
}

void GLVisualizer::setSampleRate(double newSampleRate)
{
    sampleRate = newSampleRate;
}

void GLVisualizer::setDimension(Dimension newDimension)
{
    dimension = newDimension;
    resized(); // Force a resize to update the projection matrix
}

void GLVisualizer::setTrackColourScheme(ColourScheme newColourScheme, int trackIndex)
{
    // Grow vector if trackIndex is out of range
    if (trackIndex >= (int)trackColourSchemes.size())
    {
        trackColourSchemes.resize(trackIndex + 1, rainbow);  // default to rainbow
        trackColourTextures.resize(trackIndex + 1, 0);       // matching textures
    }

    trackColourSchemes[trackIndex] = newColourScheme;
    newTextureRequested = true;
}

void GLVisualizer::setShowGrid(bool shouldShow)
{
    showGrid = shouldShow;
}

void GLVisualizer::setMinFrequency(float newMinFrequency)
{
    minFrequency = newMinFrequency;
}

void GLVisualizer::setRecedeSpeed(float newRecedeSpeed)
{
    recedeSpeed = newRecedeSpeed;
}

void GLVisualizer::setDotSize(float newDotSize)
{
    dotSize = newDotSize;
}

void GLVisualizer::setFadeEndZ(float newFadeEndZ)
{
    fadeEndZ = newFadeEndZ;
}

//=============================================================================
void GLVisualizer::startRecording()
{
    recording = true;
    resized(); // Update projection matrix
}

void GLVisualizer::stopRecording()
{
    recording = false;
    resized(); // Update projection matrix
}


//=============================================================================
/*  This is just here to keep juce::Component happy; it wants a paint()
    method because the component is marked as opaque.
*/
void GLVisualizer::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

//=============================================================================
void GLVisualizer::updateParticles()
{
    float t = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001 - startTime);
    float dt = t - lastFrameTime; // Time since last frame in seconds
    float dz = dt * recedeSpeed; // Distance receded since last frame (m)
    lastFrameTime = t;

    // Delete old particles
    while (! particles.empty())
    {
        const float age = t - particles.front().spawnTime;
        if (age * recedeSpeed < fadeEndZ)
            break; // If the oldest one is still alive, then we are done
        particles.pop_front(); // Otherwise, discard it and test the next
    }

    // Update z positions of existing particles
    for (auto& p : particles)
    {
        p.z -= dz;
    }

    {
        numTracks = results->size();

        // Add new particles from the latest analysis results
        float maxFreq = (float)sampleRate * 0.5f;
        float logMin = std::log(minFrequency);
        float logMax = std::log(maxFreq);

        float aspect = getWidth() * 1.0f / getHeight();

        for (int i = 0; i < numTracks; i++)
        {
            const auto& slot = (*results)[i];
            int activeIndex = slot.activeIndex.load(std::memory_order_acquire);
            const auto& result = slot.buffers[activeIndex];

            if (result.empty()) continue;

            int count = 0;
            for (FrequencyBand band : result)
            {
                if (band.frequency < minFrequency || band.frequency > maxFreq) 
                    continue;
                
                float x = band.pan_index * aspect;
                float y = (std::log(band.frequency) - logMin) / (logMax - logMin);
                y = juce::jmap(y, -1.0f, 1.0f);
                float z = 0.f;
                float a = band.amplitude;

                Particle newParticle = { x, y, z, a, t, (float)i };
                particles.push_back(newParticle);
                ++count;
            }
        }
    } // lock guard
}

void GLVisualizer::drawParticles(float width, float height, 
                                 juce::Matrix3D<float>& proj)
{
    using namespace juce::gl;
    auto& ext = openGLContext.extensions;

    // Set GL blending and depth testing settings
    glEnable(GL_BLEND); 
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_PROGRAM_POINT_SIZE);

    mainShader->use();

    // Set uniforms
    mainShader->setUniformMat4("uProjection", proj.mat, 1, GL_FALSE);
    mainShader->setUniformMat4("uView", view.mat, 1, GL_FALSE);
    mainShader->setUniform("uColourMap", 0);
    mainShader->setUniform("uWidth", width);
    mainShader->setUniform("uHeight", height);
    mainShader->setUniform("uFadeEndZ", fadeEndZ);
    mainShader->setUniform("uDotSize", dotSize);

    ext.glBindVertexArray(mainVAO);

    size_t offset = 0;
            
    for (int i = 0; i < numTracks; i++)
    {        
        // Build instance data array
        std::vector<InstanceData> instances;
        instances.reserve(particles.size());
        
        // Fill instances with this track's particle data
        for (auto& p : particles)
        {
            if ((int)p.trackIndex == i)
            instances.push_back({ p.spawnX, p.spawnY, p.z, p.amplitude });
        }

        // DBG("Track " << i << " will render " << instances.size() << " particles");
        
        // Upload instance data to GPU
        ext.glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        jassert((int)instances.size() <= maxParticles);
        ext.glBufferSubData(GL_ARRAY_BUFFER, 0, instances.size() * sizeof(InstanceData), instances.data());

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_1D, trackColourTextures[i]);

        // Draw all particles!!
        glDrawArraysInstanced(GL_POINTS, 0, 1, (GLsizei)instances.size());

        // Advance offset for the next track
        offset += instances.size();
    }
}

void GLVisualizer::drawGrid()
{
    using namespace juce::gl;
    auto& ext = openGLContext.extensions;

    // Upload the grid texture if needed
    if (gridTextureDirty.load() == true)
    {
        gridGLTex.release();
        gridGLTex.loadImage(gridImage);
        gridTextureDirty.store(false);
    }

    // Disable depth testing so grid is always on top
    glDisable(GL_DEPTH_TEST);

    // Set up and bind the grid shader
    gridShader->use();
    gridShader->setUniform("uTex", 1);
    glActiveTexture(GL_TEXTURE1);
    gridGLTex.bind();
    
    // Draw the grid quad
    ext.glBindVertexArray(gridVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    ext.glBindVertexArray(0);
}


//=============================================================================
/*  This function builds the 1D texture that is used to look up the
    colour corresponding to an amplitude value.
*/
/*  This function builds the 1D texture that is used to look up the
    colour corresponding to an amplitude value.
*/
void GLVisualizer::buildTexture()
{
    if (newTextureRequested == false)
        return;
    
    using namespace juce::gl;
    // auto& ext = openGLContext.extensions;

    // Define the color map data
    const int numColours = 256;
    std::vector<juce::Colour> colours(numColours);
    std::vector<float> colorData(numColours * 3);
    
    for (int i = 0; i < trackColourSchemes.size(); ++i)
    {
        // Per-track texture 
        GLuint& tex = trackColourTextures[i];
        
        // Delete old texture
        if (tex != 0)
        {
            glDeleteTextures(1, &tex);
            tex = 0;
        }
        
        // Create a 1D texture for color mapping
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_1D, tex);
        
        // Fill color data based on scheme
        ColourScheme colourScheme = trackColourSchemes[i];
        for (int j = 0; j < numColours; ++j)
        {
            float t = j / 255.0f;
            float sat = 1.0f;

            switch (colourScheme)
            {
                case greyscale:
                    colours[j] = juce::Colour::fromFloatRGBA(t, t, t, 1.0f);
                    break;

                case rainbow:
                    colours[j] = juce::Colour::fromHSV(t, sat, 1.0f, 1.0f);
                    break;

                case red:
                {
                    float hue = 0.0f; // slight shift toward orange
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.6f, 1.0f);  // high amp = brighter
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }

                case orange:
                {
                    float hue = 0.06f;
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.9f, 1.0f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }

                case yellow:
                {
                    float hue = 0.13f;
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.95f, 1.0f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }

                case lightGreen:
                {
                    float hue = 0.25f;
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.6f, 1.0f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }

                case darkGreen:
                {
                    float hue = 0.35f;
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.2f, 0.6f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }

                case lightBlue:
                {
                    float hue = 0.52;
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.8f, 1.0f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }

                case darkBlue:
                {
                    float hue = 0.63;
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.8f, 1.0f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }
                
                case purple:
                {
                    float hue = 0.8f;
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.8f, 1.0f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }

                case pink:
                {
                    float hue = 0.9f;
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.8f, 1.0f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }

                case warm:
                {
                    // Red → oragne → pale gold
                    float hue = juce::jmap(t, 0.0f, 1.0f, 0.00f, 0.13f);
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.8f, 1.0f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }

                case cool:
                {
                    // Purple → blue → green
                    float hue = juce::jmap(t, 0.0f, 1.0f, 0.85f, 0.38f);
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.8f, 1.0f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                    break;
                }

                case slider:
                {
                    float hueMax;
                    float hueMin;
                    float hue = juce::jmap(t, 0.0f, 1.0f, hueMin, hueMax);
                    float brt = juce::jmap(t, 0.0f, 1.0f, 0.8f, 1.0f);
                    colours[j] = juce::Colour::fromHSV(hue, sat, brt, 1.0f);
                }

                default:
                    jassertfalse;
                    break;
            }

            colorData[j * 3 + 0] = colours[j].getFloatRed();
            colorData[j * 3 + 1] = colours[j].getFloatGreen();
            colorData[j * 3 + 2] = colours[j].getFloatBlue();
        }

        // Upload the texture data
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, numColours, 0,
                     GL_RGB, GL_FLOAT, colorData.data());
    
        // Set texture parameters
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    newTextureRequested = false;
}

juce::Matrix3D<float> GLVisualizer::buildProjectionMatrix(float width, float height)
{
    const float aspect = width / height;
    const float fovRadians = fov * juce::MathConstants<float>::pi / 180;
    const float l = -aspect, r = +aspect;
    const float b = -1.0f, t = +1.0f;
    const float n = nearZ, f = farZ;

    juce::Matrix3D<float> proj;

    switch (dimension)
    {
    case dimension2:
        // Orthographic projection for 2D
        proj = juce::Matrix3D<float>(
            2.0f / (r - l), 0, 0, 0,
            0, 2.0f / (t - b), 0, 0,
            0, 0, -2.0f / (f - n), 0,
           -(r + l) / (r - l), -(t + b) / (t-b), -(f + n) / (f - n), 1.0f);
        break;
    
    case dimension3:
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

/*  This function ensures that the FBO used for frame capture is the 
    correct size.
*/
void GLVisualizer::updateFBOSize()
{
    if (captureFBO.isValid() == false 
     || captureFBO.getWidth() != captureW 
     || captureFBO.getHeight() != captureH)
    {
        captureFBO.release();
        captureFBO.initialise(openGLContext, captureW, captureH);

        // Readback buffer for rgb24
        capturePixels.resize(captureW * captureH * 3);
    }
}