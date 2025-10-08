#include "GLVisualizer.h"
#include "MainController.h"


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

void GLVisualizer::prepareToPlay(int newSamplesPerBlock, double newSampleRate)
{
    sampleRate = newSampleRate;
    juce::ignoreUnused(newSamplesPerBlock);
}
//=============================================================================
void GLVisualizer::buildTexture()
{
    using namespace juce::gl;
    // auto& ext = openGLContext.extensions;

    if (colourMapTex != 0)
        glDeleteTextures(1, &colourMapTex);

    // Create a 1D texture for color mapping
    glGenTextures(1, &colourMapTex);
    glBindTexture(GL_TEXTURE_1D, colourMapTex);

    // Define the color map data
    const int numColours = 256;
    std::vector<juce::Colour> colours(numColours);
    
    switch (colourScheme)
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

    default:
        jassertfalse; // Unsupported colour scheme
        break;
    }

    std::vector<float> colorData(numColours * 3);
    
    for (int i = 0; i < numColours; ++i)
    {
        colorData[i * 3 + 0] = colours[i].getFloatRed();
        colorData[i * 3 + 1] = colours[i].getFloatGreen();
        colorData[i * 3 + 2] = colours[i].getFloatBlue();
    }

    // Upload the texture data
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, numColours, 0,
                 GL_RGB, GL_FLOAT, colorData.data());

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

//=============================================================================
void GLVisualizer::initialise() 
{
    using namespace juce::gl;
    auto& ext = openGLContext.extensions;

    // GLSL vertex shader initialization code
    static const char* vertSrc = R"(#version 150
        in vec4 instanceData;

        uniform mat4 uProjection;
        uniform mat4 uView;
        uniform sampler1D uColourMap;
        uniform float uFadeEndZ;
        uniform float uDotSize;
        uniform float uAmpScale;

        out vec4 vColour;
        
        void main()
        {
            // Root-scale the amplitude
            float amp = pow(instanceData.w, 1.0 / uAmpScale);

            // Depth factor for fading effect
            float depth = -instanceData.z / uFadeEndZ;
            float alpha = amp * (1.0 - depth);
            
            // Look up color from the texture
            vec3 rgb = texture(uColourMap, amp).rgb;

            // Set the color with alpha
            vColour = vec4(rgb, alpha);

            // Build world position
            vec4 worldPos = vec4(instanceData.xyz, 1.0);

            // Compute clip-space coordinate
            gl_Position = uProjection * uView * worldPos;

            // Size in pixels
            gl_PointSize = (50.0 + 50.0 * amp) * uDotSize ;
        }
    )";

    // GLSL fragment shader initialization code
    static const char* fragSrc = R"(#version 150
        in vec4 vColour;
        out vec4 frag;

        void main()
        {
            // Remap coordinates to [-1, +1]
            vec2 p = gl_PointCoord * 2.0 - 1.0;

            // Discard fragments outside the circle
            if (dot(p, p) > 1.0) discard;

            // Set the fragment colour!
            frag = vec4(vColour);
        }
    )";
    
    // Compile and link the shaders
    shader.reset(new juce::OpenGLShaderProgram(openGLContext));
    shader->addVertexShader(vertSrc);
    shader->addFragmentShader(fragSrc);

    ext.glBindAttribLocation(shader->getProgramID(), 0, "instanceData");

    bool shaderLinked = shader->link();
    jassert(shaderLinked);
    juce::ignoreUnused(shaderLinked);

    buildTexture();

    // Generate and bind the vertex-array object
    ext.glGenVertexArrays(1, &vao);
    ext.glBindVertexArray(vao);

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
}

void GLVisualizer::shutdown()
{
    auto& ext = openGLContext.extensions;
    
    // Delete VAO and VBOs
    if (vao != 0) 
    { 
        ext.glDeleteVertexArrays(1, &vao); 
        vao = 0; 
    }
    if (vbo.id != 0) 
    { 
        ext.glDeleteBuffers(1, &vbo.id); 
        vbo.id = 0; 
    }
    if (instanceVBO != 0) 
    {
        ext.glDeleteBuffers(1, &instanceVBO);
        instanceVBO = 0;
    }

    shader.reset();
}

void GLVisualizer::render()
{
    using namespace juce::gl;
    auto& ext = openGLContext.extensions;

    // Enable blending and depth testing
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Check if we need to rebuild the texture
    if (newTextureRequsted)
    {
        buildTexture();
        newTextureRequsted = false;
    }

    // Clear to black
    juce::OpenGLHelpers::clear(juce::Colours::black);
    glClear(GL_DEPTH_BUFFER_BIT); 

    if (shader == nullptr) return; // Early-out on link failure

    shader->use();

    float t = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001 
                    - startTime);

    float dt = t - lastFrameTime;
    float dz = dt * recedeSpeed;
    lastFrameTime = t;

    auto results = controller.getLatestResults();
    // DBG("New results received. Size = " << results.size());

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
        p.z -= dz;

    // Add new particles from the latest analysis results
    if (!results.empty())
    {
        float maxFreq = (float)sampleRate * 0.5f;
        float logMin = std::log(minFrequency);
        float logMax = std::log(maxFreq);

        float aspect = getWidth() * 1.0f / getHeight();

        // For determining the amplitude range
        // float minAmp = std::numeric_limits<float>::max();
        // float maxAmp = 0.f;

        for (frequency_band band : results)
        {
            if (band.frequency < minFrequency || band.frequency > maxFreq) 
                continue;
            
            float x = band.pan_index * aspect;
            float y = (std::log(band.frequency) - logMin) / (logMax - logMin);
            y = juce::jmap(y, -1.0f, 1.0f);
            float z = 0.f;
            float a = band.amplitude;

            Particle newParticle = { x, y, z, a, t };
            particles.push_back(newParticle);

            // if (a < minAmp)
            //     minAmp = a;

            // if (a > maxAmp)
            //     maxAmp = a;

            // if (band.frequency > 500.f && band.frequency < 600.f)
            // DBG("Added new particle for frequency " << band.frequency << ": "
            //     << "x = " << x << ", y = " << y << ", a = " << a);
        }

        // DBG("Amplitude range: [" << minAmp << ", " << maxAmp << "]");
    }

    // Build instance data array
    std::vector<InstanceData> instances;
    instances.reserve(particles.size());
    for (auto& p : particles)
        instances.push_back({ p.spawnX, p.spawnY, p.z, p.spawnAlpha });

    // Upload instance data to GPU
    ext.glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    jassert((int)instances.size() <= maxParticles);
    ext.glBufferSubData(GL_ARRAY_BUFFER, 0,
                        instances.size() * sizeof(InstanceData),
                        instances.data());

    // Set uniforms
    shader->setUniformMat4("uProjection", projection.mat, 1, GL_FALSE);
    shader->setUniformMat4("uView", view.mat, 1, GL_FALSE);
    shader->setUniform("uColourMap", 0);
    shader->setUniform("uFadeEndZ", fadeEndZ);
    shader->setUniform("uDotSize", dotSize);
    shader->setUniform("uAmpScale", ampScale);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, colourMapTex);

    // Draw all particles!!
    ext.glBindVertexArray(vao);
    glDrawArraysInstanced(GL_POINTS, 0, 1, (GLsizei)instances.size());

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        DBG("OpenGL error: " << juce::String::toHexString((int)err));
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
    case Dim2D:
        // Orthographic projection for 2D
        projection = juce::Matrix3D<float>(
            2.0f / (r - l), 0, 0, 0,
            0, 2.0f / (t - b), 0, 0,
            0, 0, -2.0f / (f - n), 0,
           -(r + l) / (r - l), -(t + b) / (t-b), -(f + n) / (f - n), 1.0f);
        break;
    
    case Dim3D:
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

//=============================================================================
void GLVisualizer::setDimension(Dimension newDimension)
{
    dimension = newDimension;
    resized(); // Force a resize to update the projection matrix
}

void GLVisualizer::setColourScheme(ColourScheme newColourScheme)
{
    colourScheme = newColourScheme;
    newTextureRequsted = true;
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

/*  This is here just to keep juce::Component happy, it wants a paint()
    method because the component is marked as opaque.
*/
void GLVisualizer::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}