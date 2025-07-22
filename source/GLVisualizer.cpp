#include "GLVisualizer.h"
#include "MainController.h"


//=============================================================================
GLVisualizer::GLVisualizer(MainController& c) : controller(c) 
{
    juce::ignoreUnused(c);

    auto glVersion = juce::OpenGLContext::OpenGLVersion::openGL3_2;
    openGLContext.setOpenGLVersionRequired(glVersion);
    openGLContext.setContinuousRepainting(true);

    startTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
}

GLVisualizer::~GLVisualizer()
{
    openGLContext.setContinuousRepainting(false);
    openGLContext.detach();
}
//=============================================================================
void GLVisualizer::setRecedeSpeed(float newRecedeSpeed)
{
    recedeSpeed = newRecedeSpeed;
}

void GLVisualizer::setDotSize(float newDotSize)
{
    dotSize = newDotSize;
}

void GLVisualizer::setNearZ(float newNearZ)
{
    nearZ = newNearZ;
}

void GLVisualizer::setFadeEndZ(float newFadeEndZ)
{
    fadeEndZ = newFadeEndZ;
}

void GLVisualizer::setFarZ(float newFarZ)
{
    farZ = newFarZ;
}

void GLVisualizer::setFOV(float newFOV)
{
    fov = newFOV;
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
        uniform float uCurrentTime;
        uniform float uSpeed;
        uniform float uFadeEndZ;
        uniform float uDotSize;

        out float vDepth;
        out float vSpawnAlpha;

        void main()
        {
            // Compute age-based z
            float age = uCurrentTime - instanceData.z;
            float z = -age * uSpeed;

            // Pass depth factor for fade and spawn alpha
            vDepth = -z / uFadeEndZ;

            // Log-scale the amplitude
            // float uK = 100.0; // Logarithmic scaling factor
            // vSpawnAlpha = log(1.0 + uK * instanceData.w) / log(1.0 + uK);

            // Root-scale the amplitude
            float d = 10.0; // Root scaling factor
            vSpawnAlpha = pow(instanceData.w, 1.0 / d);

            // Build world position
            vec4 worldPos = vec4(instanceData.xy, z, 1.0);

            // Compute clip-space coordinate
            gl_Position = uProjection * uView * worldPos;

            // Size in pixels
            gl_PointSize = 100.0 * uDotSize * sqrt(instanceData.w);
            // gl_PointSize = 20.0;
        }
    )";

    // GLSL fragment shader initialization code
    static const char* fragSrc = R"(#version 150
        in float vDepth;        
        in float vSpawnAlpha;
        out vec4 frag;

        void main()
        {
            // remap to -1..+1
            vec2 p = gl_PointCoord * 2.0 - 1.0;
            if (dot(p, p) > 1.0) discard;

            float alpha = vSpawnAlpha * (1.0 - vDepth);
            frag = vec4(1.0, 1.0, 1.0, alpha);
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

    // Force repaint to make sure the mvp is correct - try this if stuff breaks
    // resized();
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

    // Clear to black
    juce::OpenGLHelpers::clear(juce::Colours::black);
    glClear(GL_DEPTH_BUFFER_BIT); 

    if (shader == nullptr) return; // early-out on link failure

    shader->use();

    float t = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001 
                    - startTime);

    // First draft of audio-based visuals
    auto results = controller.getLatestResults();

    if (!results.empty())
    {
        float sampleRate = static_cast<float>(controller.getSampleRate());
        float minFreq = 20.f; // Hardcoded for now
        float maxFreq = sampleRate * 0.5f;
        float minBandFreq = 0;

        for (frequency_band band : results)
        {
            if (band.frequency > minFreq)
            {
                minBandFreq = band.frequency;
                break; // Assuming frequencies in sorted order
            }
        }

        float logMin = std::log(minBandFreq);
        float logMax = std::log(maxFreq);

        float aspect = getWidth() * 1.0f / getHeight();

        // For determining the amplitude range
        float minAmp = std::numeric_limits<float>::max();
        float maxAmp = 0.f;

        for (frequency_band band : results)
        {
            if (band.frequency < minBandFreq || band.frequency > maxFreq) 
                continue;
            
            float x = band.pan_index * aspect;
            float y = (std::log(band.frequency) - logMin) / (logMax - logMin);
            y = juce::jmap(y, -1.0f, 1.0f);
            float a = band.amplitude;

            Particle newParticle = { x, y, t, a};
            particles.push_back(newParticle);

            if (a < minAmp)
                minAmp = a;

            if (a > maxAmp)
                maxAmp = a;

            // DBG("Added new particle for frequency " << band.frequency << ": "
            //     << "x = " << x << ", y = " << y << ", a = " << a);
        }

        DBG("Amplitude range: [" << minAmp << ", " << maxAmp << "]");
    }

    // Delete old particles
    while (!particles.empty())
    {
        const float age = t - particles.front().spawnTime;
        if (age * recedeSpeed < fadeEndZ)
            break; // If the oldest one is still alive, then we are done
        particles.pop_front(); // Otherwise, discard it and test the next
    }

    // Build instance data array
    std::vector<InstanceData> instances;
    instances.reserve(particles.size());
    for (auto& p : particles)
        instances.push_back({ p.spawnX, p.spawnY, p.spawnTime, p.spawnAlpha });

    // Upload instance data to GPU
    ext.glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    jassert(instances.size() <= maxParticles);
    ext.glBufferSubData(GL_ARRAY_BUFFER, 0,
                        instances.size() * sizeof(InstanceData),
                        instances.data());

    // Set uniforms
    shader->setUniformMat4("uProjection", projection.mat, 1, GL_FALSE);
    shader->setUniformMat4("uView", view.mat, 1, GL_FALSE);
    shader->setUniform("uCurrentTime", t);
    shader->setUniform("uSpeed", recedeSpeed);
    shader->setUniform("uFadeEndZ", fadeEndZ);
    shader->setUniform("uDotSize", dotSize);

    // DBG("Dot Size = " << dotSize);

    // Draw all particles!!
    ext.glBindVertexArray(vao);
    glDrawArraysInstanced(GL_POINTS, 0, 1, (GLsizei)instances.size());

    // ext.glBindVertexArray(0);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        DBG("OpenGL error: " << juce::String::toHexString((int)err));
        jassertfalse; // triggers the assertion so you still get a breakpoint
    }
}

void GLVisualizer::resized() 
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();
    const float aspect = w / h;
    const float fovRadians = fov * juce::MathConstants<float>::pi / 180;
    
    view = juce::Matrix3D<float>::fromTranslation(cameraPosition);
    projection = juce::Matrix3D<float>::fromFrustum(
              -nearZ * std::tan (fovRadians * 0.5f) * aspect,   // left
               nearZ * std::tan (fovRadians * 0.5f) * aspect,   // right
              -nearZ * std::tan (fovRadians * 0.5f),            // bottom
               nearZ * std::tan (fovRadians * 0.5f),            // top
               nearZ, farZ);
}