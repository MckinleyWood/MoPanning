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
}

//=============================================================================
/*  This function builds the 1D texture that is used to convert look up 
    colour for amplitude value.
*/
void GLVisualizer::buildTexture()
{
    if (!newTextureRequested.load())
        return;

    using namespace juce::gl;

    // Delete any old textures that are no longer needed
    for (GLuint& tex : trackColourTextures)
    {
        if (tex != 0)
        {
            glDeleteTextures (1, &tex);
            tex = 0;
        }
    }

    const int numColours = 256;
    std::vector<float> colorData (numColours * 3);

    // Build a texture for **every** track that currently exists
    for (size_t track = 0; track < trackColourSchemes.size(); ++track)
    {
        const ColourScheme scheme = trackColourSchemes[track];

        // ----- fill colour array ------------------------------------------------
        std::vector<juce::Colour> colours (numColours);
        switch (scheme)
        {
            case greyscale:
                for (int i = 0; i < numColours; ++i)
                    colours[i] = juce::Colour((juce::uint8)i, (juce::uint8)i, (juce::uint8)i);
                break;
            case rainbow:
                for (int i = 0; i < numColours; ++i)
                    colours[i] = juce::Colour::fromHSV((float)i / numColours, 
                                                       1.0f, 1.0f, 1.0f);
                break;
            case red:
                for (int i = 0; i < numColours; ++i)
                    colours[i] = juce::Colour::fromFloatRGBA((float)i/numColours, 0.0f, 0.0f, 1.0f);
                break;

            case green:
                for (int i = 0; i < numColours; ++i)
                    colours[i] = juce::Colour::fromFloatRGBA(0.0f, (float)i/numColours, 0.0f, 1.0f);
                break;

            case blue:
                for (int i = 0; i < numColours; ++i)
                    colours[i] = juce::Colour::fromFloatRGBA(0.0f, 0.0f, (float)i/numColours, 1.0f);
                break;

            case warm:
                for (int i = 0; i < numColours; ++i)
                    colours[i] = juce::Colour::fromHSV(0.05f + 0.1f * (float)i / numColours, 1.0f, 1.0f, 1.0f);
                break;

            case cool:
                for (int i = 0; i < numColours; ++i)
                    colours[i] = juce::Colour::fromHSV(0.6f + 0.1f * (float)i / numColours, 1.0f, 1.0f, 1.0f);
                break;

            default:
                jassertfalse;
                break;
        }

        for (int i = 0; i < numColours; ++i)
        {
            colorData[i * 3 + 0] = colours[i].getFloatRed();
            colorData[i * 3 + 1] = colours[i].getFloatGreen();
            colorData[i * 3 + 2] = colours[i].getFloatBlue();
        }

        // ----- create GL texture ------------------------------------------------
        GLuint tex = 0;
        glGenTextures (1, &tex);
        glBindTexture (GL_TEXTURE_1D, tex);
        glTexImage1D (GL_TEXTURE_1D, 0, GL_RGB, numColours, 0,
                      GL_RGB, GL_FLOAT, colorData.data());

        glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        trackColourTextures[track] = tex;
    }

    newTextureRequested.store (false);
}

//=============================================================================
/*  This function is called when the OpenGL context is created. It is
    where we initialize all of our GL resources, including the shaders
    for the main dot cloud and the grid overlay, the colourmap texture,
    

*/
void GLVisualizer::initialise()
{
    using namespace juce::gl;
    auto& ext = openGLContext.extensions;

    // 1. Shaders
    static const char* mainVertSrc = R"(#version 150
        in vec4 instanceData;
        in float instanceTrackIndex;

        uniform mat4 uProjection;
        uniform mat4 uView;
        uniform sampler1D uColourMap;
        uniform float uFadeEndZ;
        uniform float uDotSize;
        uniform float uAmpScale;

        out vec4 vColour;

        void main()
        {
            float amp   = pow(instanceData.w, 1.0 / uAmpScale);
            float depth = -instanceData.z / uFadeEndZ;
            float alpha = amp * (1.0 - depth);
            vec3  rgb   = texture(uColourMap, amp).rgb;
            vColour     = vec4(rgb, alpha);

            vec4 worldPos = vec4(instanceData.xyz, 1.0);
            gl_Position   = uProjection * uView * worldPos;
            gl_PointSize  = (50.0 + 50.0 * amp) * uDotSize;
        }
    )";

    static const char* mainFragSrc = R"(#version 150
        in vec4 vColour;
        out vec4 frag;
        void main()
        {
            vec2 p = gl_PointCoord * 2.0 - 1.0;
            if (dot(p, p) > 1.0) discard;
            frag = vColour;
        }
    )";

    mainShader.reset (new juce::OpenGLShaderProgram (openGLContext));
    mainShader->addVertexShader   (mainVertSrc);
    mainShader->addFragmentShader (mainFragSrc);
    ext.glBindAttribLocation (mainShader->getProgramID(), 0, "instanceData");
    ext.glBindAttribLocation (mainShader->getProgramID(), 1, "instanceTrackIndex");
    jassert (mainShader->link());

    // 2. Initialise per-track containers (start with 1 track)
    trackColourTextures.clear();
    trackColourSchemes.clear();
    trackParticleCounts.clear();
    trackParticleOffsets.clear();

    trackColourTextures.push_back (0);
    trackColourSchemes .push_back (rainbow);   // default scheme
    trackParticleCounts.push_back (0);
    trackParticleOffsets.push_back (0);

    newTextureRequested.store (true);   // force first texture build

    // 3. VAO / VBO for the dot cloud
    ext.glGenVertexArrays (1, &mainVAO);
    ext.glBindVertexArray (mainVAO);

    ext.glGenBuffers (1, &instanceVBO);
    ext.glBindBuffer (GL_ARRAY_BUFFER, instanceVBO);
    ext.glBufferData (GL_ARRAY_BUFFER,
                      maxParticles * sizeof (InstanceData),
                      nullptr,
                      GL_DYNAMIC_DRAW);

    // attribute 0 = vec4 (x, y, z, alpha)
    ext.glEnableVertexAttribArray (0);
    ext.glVertexAttribPointer (0, 4, GL_FLOAT, GL_FALSE,
                               sizeof (InstanceData), (void*)0);
    glVertexAttribDivisor (0, 1);

    // attribute 1 = float trackIndex (offset = 4 * sizeof(float))
    ext.glEnableVertexAttribArray (1);
    ext.glVertexAttribPointer (1, 1, GL_FLOAT, GL_FALSE,
                               sizeof (InstanceData),
                               (void*)(4 * sizeof (float)));
    glVertexAttribDivisor (1, 1);

    ext.glBindVertexArray (0);

    // 4. Grid shader / VAO / VBO (unchanged)
    static const char* gridVertSrc = R"(#version 150
        in vec2 aPos;
        in vec2 aUV;
        out vec2 vUV;
        void main() { vUV = aUV; gl_Position = vec4(aPos,0.0,1.0); }
    )";

    static const char* gridFragSrc = R"(#version 150
        uniform sampler2D uTex;
        in vec2 vUV;
        out vec4 fragColor;
        void main() { fragColor = texture(uTex, vUV); }
    )";

    gridShader.reset (new juce::OpenGLShaderProgram (openGLContext));
    gridShader->addVertexShader   (gridVertSrc);
    gridShader->addFragmentShader (gridFragSrc);
    jassert (gridShader->link());

    ext.glGenVertexArrays (1, &gridVAO);
    ext.glBindVertexArray (gridVAO);
    ext.glGenBuffers (1, &gridVBO);
    ext.glBindBuffer (GL_ARRAY_BUFFER, gridVBO);

    static const float quadData[] = {
        -1.f, -1.f, 0.f, 1.f,
         1.f, -1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 0.f,
         1.f,  1.f, 1.f, 0.f
    };
    glBufferData (GL_ARRAY_BUFFER, sizeof (quadData), quadData, GL_STATIC_DRAW);

    GLint posLoc = ext.glGetAttribLocation (gridShader->getProgramID(), "aPos");
    GLint uvLoc  = ext.glGetAttribLocation (gridShader->getProgramID(), "aUV");
    glEnableVertexAttribArray ((GLuint)posLoc);
    glEnableVertexAttribArray ((GLuint)uvLoc);
    glVertexAttribPointer ((GLuint)posLoc, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glVertexAttribPointer ((GLuint)uvLoc,  2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

    ext.glBindVertexArray (0);
    ext.glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void GLVisualizer::shutdown()
{
    auto& ext = openGLContext.extensions;

    // ---- delete per-track colour-map textures ----
    for (GLuint tex : trackColourTextures)
        if (tex != 0) juce::gl::glDeleteTextures(1, &tex);
    trackColourTextures.clear();

    // ---- VAOs / VBOs ----
    if (mainVAO)    { ext.glDeleteVertexArrays (1, &mainVAO);   mainVAO = 0; }
    if (instanceVBO){ ext.glDeleteBuffers (1, &instanceVBO);    instanceVBO = 0; }
    if (gridVBO)    { ext.glDeleteBuffers (1, &gridVBO);        gridVBO = 0; }
    if (gridVAO)    { ext.glDeleteVertexArrays (1, &gridVAO);   gridVAO = 0; }

    gridGLTex.release();

    mainShader.reset();
    gridShader.reset();
}

void GLVisualizer::render()
{
    using namespace juce::gl;
    auto& ext = openGLContext.extensions;

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);
    glEnable (GL_PROGRAM_POINT_SIZE);

    // Re-build colour-map textures if a new track appeared
    buildTexture();      // <-- handles newTextureRequsted

    // Clear
    juce::OpenGLHelpers::clear (juce::Colours::black);
    glClear (GL_DEPTH_BUFFER_BIT);

    if (mainShader == nullptr) return;   // shader link failed

    mainShader->use();

    // Time handling
    float t  = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001 - startTime);
    float dt = t - lastFrameTime;
    float dz = dt * recedeSpeed;
    lastFrameTime = t;

    // Get per-track analysis results
    auto& results = controller.getLatestResults();
    DBG("Results vector size in GLVis: " << results.size());
    for (int i=0; i< results.size(); i++)
        DBG("Track " << i << ": " << results[i].size());

    // ---- 1. Prune old particles ---------------------------
    while (!particles.empty())
    {
        const float age = t - particles.front().spawnTime;
        if (age * recedeSpeed < fadeEndZ)
            break;
        particles.pop_front();
    }

    // ---- 2. Move existing particles back in Z -------------------------
    for (auto& p : particles)
        p.z -= dz;

    // ---- 3. Spawn new particles from the latest results ---------------
    if (!results.empty())
    {
        const float maxFreq = (float)sampleRate * 0.5f;
        const float logMin  = std::log (minFrequency);
        const float logMax  = std::log (maxFreq);
        const float aspect  = getWidth() * 1.0f / getHeight();

        for (int track = 0; track < results.size(); ++track)
        {
            const auto trackResults = results[track];

            for (const frequency_band& band : trackResults)
            {
                if (band.frequency < minFrequency || band.frequency > maxFreq)
                    continue;

                const float x = band.pan_index * aspect;
                const float y = juce::jmap ((std::log (band.frequency) - logMin) / (logMax - logMin),
                                        -1.0f, 1.0f);
                const float a = band.amplitude;

                Particle p{ x, y, 0.0f, a, t, float(band.trackIndex) };
                particles.push_back (p);
            }
        }
    }

    // ---- 4. Determine highest track index in the current particle set
    int maxTrack = results.size();

    // ---- 5. Resize per-track containers if needed --------------------
    if (maxTrack >= 0 && (trackColourTextures.size() <= (size_t)maxTrack ||
                          trackColourSchemes.size()  <= (size_t)maxTrack))
    {
        const size_t newSize = maxTrack + 1;
        trackColourTextures.resize (newSize, 0);
        trackColourSchemes .resize (newSize, greyscale);
        trackParticleCounts.resize (newSize, 0);
        trackParticleOffsets.resize (newSize, 0);
        newTextureRequested = true;          // forces buildTexture() next frame
        DBG ("Resized track containers to " << newSize);
    }

    // ---- 6. Count particles per track --------------------------------
    std::fill (trackParticleCounts.begin(), trackParticleCounts.end(), 0);
    for (const auto& p : particles)
    {
        const int trk = (int)p.trackIndex;
        if (trk >= 0 && trk < (int)trackParticleCounts.size())
            ++trackParticleCounts[trk];
    }

    // ---- 7. Compute offsets (so we can draw each track in one call) --
    size_t offset = 0;
    for (size_t i = 0; i < trackParticleCounts.size(); ++i)
    {
        trackParticleOffsets[i] = offset;
        offset += trackParticleCounts[i];
    }

    // ---- 8. Build instance data ----------------------
    std::vector<InstanceData> instances;
    instances.reserve (particles.size());
    for (const auto& p : particles)
        instances.push_back ({ p.spawnX, p.spawnY, p.z, p.spawnAlpha, p.trackIndex });

    // ---- 9. Upload instance VBO ---------------------------------------
    ext.glBindBuffer (GL_ARRAY_BUFFER, instanceVBO);
    jassert ((int)instances.size() <= maxParticles);
    ext.glBufferSubData (GL_ARRAY_BUFFER, 0,
                         instances.size() * sizeof (InstanceData),
                         instances.data());

    // ---- 10. Uniforms (unchanged) ------------------------------------
    mainShader->setUniformMat4 ("uProjection", projection.mat, 1, GL_FALSE);
    mainShader->setUniformMat4 ("uView",      view.mat,      1, GL_FALSE);
    mainShader->setUniform ("uColourMap", 0);
    mainShader->setUniform ("uFadeEndZ", fadeEndZ);
    mainShader->setUniform ("uDotSize",  dotSize);
    mainShader->setUniform ("uAmpScale", ampScale);

    // 11. Draw ONE CALL PER TRACK (replace the old single draw)
    ext.glBindVertexArray (mainVAO);

    for (size_t track = 0; track < trackColourTextures.size(); ++track)
    {
        if (trackParticleCounts[track] == 0)
            continue;

        // Safety – texture may still be 0 on the very first frame after resize
        if (trackColourTextures[track] == 0)
            continue;

        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_1D, trackColourTextures[track]);

        glDrawArraysInstanced (GL_POINTS,
                            (GLsizei)trackParticleOffsets[track],
                            1,
                            (GLsizei)trackParticleCounts[track]);
    }
    ext.glBindVertexArray (0);

    // ---- 12. Grid overlay --------------------------------
    if (showGrid)
    {
        if (gridTextureDirty.load())
        {
            gridGLTex.release();
            gridGLTex.loadImage (gridImage);
            gridTextureDirty.store (false);
        }

        glDisable (GL_DEPTH_TEST);

        gridShader->use();
        gridShader->setUniform ("uTex", 1);
        glActiveTexture (GL_TEXTURE1);
        gridGLTex.bind();

        ext.glBindVertexArray (gridVAO);
        glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
        ext.glBindVertexArray (0);
    }

    // ---- 13. OpenGL error check -------------------------
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        DBG ("OpenGL error: " << juce::String::toHexString ((int)err));
}

void GLVisualizer::resized() 
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();
    const float aspect = w / h;
    const float fovRadians = fov * juce::MathConstants<float>::pi / 180;
    const float l = -aspect, r = +aspect;
    const float b = -1.0f, t = +1.0f;
    const float n = nearZ, f = farZ;
    
    view = juce::Matrix3D<float>::fromTranslation(cameraPosition);

    switch (dimension)
    {
    case dimension2:
        // Orthographic projection for 2D
        projection = juce::Matrix3D<float>(
            2.0f / (r - l), 0, 0, 0,
            0, 2.0f / (t - b), 0, 0,
            0, 0, -2.0f / (f - n), 0,
           -(r + l) / (r - l), -(t + b) / (t-b), -(f + n) / (f - n), 1.0f);
        break;
    
    case dimension3:
        // Perspective projection for 3D
        projection = juce::Matrix3D<float>::fromFrustum(
              -nearZ * std::tan(fovRadians * 0.5f) * aspect,   // left
               nearZ * std::tan(fovRadians * 0.5f) * aspect,   // right
              -nearZ * std::tan(fovRadians * 0.5f),            // bottom
               nearZ * std::tan(fovRadians * 0.5f),            // top
               nearZ, farZ);
        break;
    
    default:
        jassertfalse; // Unknown dimension
    }
    
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
void GLVisualizer::setSampleRate(double newSampleRate)
{
    sampleRate = newSampleRate;
}

void GLVisualizer::setDimension(Dimension newDimension)
{
    dimension = newDimension;
    resized(); // Force a resize to update the projection matrix
}

void GLVisualizer::setTrackColourScheme (ColourScheme newScheme, int trackIndex)
{
    if (trackIndex < 0) return;

    // Grow containers if the UI asks for a track that does not exist yet
    if ((size_t)trackIndex >= trackColourSchemes.size())
    {
        size_t newSize = trackIndex + 1;
        trackColourTextures.resize (newSize, 0);
        trackColourSchemes .resize (newSize, greyscale);
        trackParticleCounts.resize (newSize, 0);
        trackParticleOffsets.resize (newSize, 0);
    }

    trackColourSchemes[trackIndex] = newScheme;
    newTextureRequested.store (true);   // rebuild textures next frame
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

void GLVisualizer::setAmpScale(float newAmpScale)
{
    ampScale = newAmpScale;
}

void GLVisualizer::setFadeEndZ(float newFadeEndZ)
{
    fadeEndZ = newFadeEndZ;
}

/*  This is just here to keep juce::Component happy; it wants a paint()
    method because the component is marked as opaque.
*/
void GLVisualizer::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}