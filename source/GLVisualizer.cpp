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
void GLVisualizer::initialise() 
{
    using namespace juce::gl;
    auto& ext = openGLContext.extensions;

    // GLSL vertex shader initialization code
    static const char* vertSrc = R"(#version 150
        in vec2 position;
        in vec4 instanceData;

        uniform mat4 uProjection;
        uniform mat4 uView;
        uniform float uCurrentTime;
        uniform float uSpeed;
        uniform float uFadeEndZ;
        uniform float uDotSize;

        out vec2 vPos;
        out float vDepth;
        out float vSpawnAlpha;

        void main()
        {
            // Compute age-based z
            float age = uCurrentTime - instanceData.z;
            float z = -age * uSpeed;

            // Pass depth factor for fade and spawn alpha
            vDepth = -z / uFadeEndZ;
            vSpawnAlpha = instanceData.w;

            // Build world position
            vec3 spawnPos = vec3(instanceData.x, instanceData.y, 0.0);
            vec4 localPos = vec4(position, 0.0, 1.0);
            vec4 worldPos = vec4(spawnPos, 1.0) + vec4(localPos.xyz, 0.0);
            worldPos.z += z; // apply receding

            // Compute clip-space coordinate
            gl_Position = uProjection * uView * worldPos;

            // Pass position scaled appropriately
            vPos = position / uDotSize;
        }
    )";

    // GLSL fragment shader initialization code
    static const char* fragSrc = R"(#version 150
        in vec2 vPos;
        in float vDepth;        
        in float vSpawnAlpha;
        out vec4 frag;

        void main()
        {
            // Circular mask
            if (dot(vPos, vPos) > 1.0)
                discard;

            // Fade out with distance
            float alpha = vSpawnAlpha * (1.0 - vDepth); 
            frag = vec4(1.0, 1.0, 1.0, alpha);
        }
    )";
    
    // Compile and link the shaders
    shader.reset(new juce::OpenGLShaderProgram(openGLContext));
    shader->addVertexShader(vertSrc);
    shader->addFragmentShader(fragSrc);

    ext.glBindAttribLocation(shader->getProgramID(), 0, "position");
    ext.glBindAttribLocation(shader->getProgramID(), 1, "instanceData");

    bool shaderLinked = shader->link();
    jassert(shaderLinked);
    juce::ignoreUnused(shaderLinked);

    // Build a 2×2 clip-space quad centred at (0,0)
    vbo.create(openGLContext);
    ext.glBindBuffer(GL_ARRAY_BUFFER, vbo.id);

    const float quad[8] = {
        -dotSize, -dotSize,
         dotSize, -dotSize,
        -dotSize,  dotSize,
         dotSize,  dotSize
    };
    ext.glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    // Generate and bind the vertex-array object
    ext.glGenVertexArrays(1, &vao);
    ext.glBindVertexArray(vao);

    // Tell the VAO about our position attribute
    ext.glBindBuffer(GL_ARRAY_BUFFER, vbo.id);
    ext.glEnableVertexAttribArray(0);
    ext.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Create instance buffer
    ext.glGenBuffers(1, &instanceVBO);

    // Bind the instanceVBO to set up the attribute pointer for instancing.
    ext.glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    ext.glEnableVertexAttribArray(1);
    ext.glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 
                              sizeof(InstanceData), nullptr);
    glVertexAttribDivisor(1, 1); // This attribute advances once per instance

    // Unbind VAO to avoid accidental state leakage:
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

    // Clear to black
    juce::OpenGLHelpers::clear(juce::Colours::black);
    glClear(GL_DEPTH_BUFFER_BIT); 

    if (shader == nullptr) return; // early-out on link failure

    shader->use();

    float t = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001 
                    - startTime);

    // First draft of audio-based visuals
    auto results = controller.getLatestResults();
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

    if (!results.empty())
    {
        // DBG("Reading from results...");

        for (frequency_band band : results)
        {
            if (band.frequency < minBandFreq || band.frequency > maxFreq) 
                continue;

            float x = band.pan_index;
            float y = (std::log(band.frequency) - logMin) / (logMax - logMin);
            y = juce::jmap(y, -1.0f, 1.0f);
            float a = band.amplitude;

            Particle newParticle = { x, y, t, a};
            particles.push_back(newParticle);

            DBG("Added new particle for frequency " << band.frequency << ": "
                << "x = " << x << ", y = " << y << ", a = " << a);
        }
    }

    // Spirals
    // Compute position for the new particles
    // float t = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001 
    //                 - startTime);
    // float r = 0.75f;     
    // float w = juce::MathConstants<float>::twoPi * 0.4f;
    // float x = r * std::cos(w * t);
    // float y = r * std::sin(w * t);

    // // Add new particles to the queue
    // Particle newParticle1 = { x, y, t, avg_amplitude};
    // Particle newParticle2 = { -x, -y, t, avg_amplitude};
    // particles.push_back(newParticle1);
    // particles.push_back(newParticle2);


    // Delete old particles
    while (!particles.empty())
    {
        const float age = t - particles.front().spawnTime;
        if (age * speed < fadeEndZ)   // still visible?
            break;                    // oldest one is still alive
        particles.pop_front();        // otherwise discard and test next
    }

    // Build instance data array
    std::vector<InstanceData> instances;
    instances.reserve(particles.size());
    for (auto& p : particles)
        instances.push_back({ p.spawnX, p.spawnY, p.spawnTime, p.spawnAlpha });

    // Upload instance data to GPU
    ext.glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    ext.glBufferData(GL_ARRAY_BUFFER,
                     (GLsizeiptr)(instances.size() * sizeof(InstanceData)),
                     instances.data(),
                     GL_DYNAMIC_DRAW);

    // Set uniforms
    shader->setUniformMat4("uProjection", projection.mat, 1, GL_FALSE);
    shader->setUniformMat4("uView", view.mat, 1, GL_FALSE);
    shader->setUniform("uCurrentTime", t);
    shader->setUniform("uSpeed", speed);
    shader->setUniform("uFadeEndZ", fadeEndZ);
    shader->setUniform("uDotSize", dotSize);

    // Draw all particles!!
    ext.glBindVertexArray(vao);
    GLsizei instanceCount = static_cast<GLsizei>(instances.size());
    if (instanceCount > 0)
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instanceCount);
    ext.glBindVertexArray(0);

    jassert(glGetError() == GL_NO_ERROR);
}

void GLVisualizer::resized() 
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();
    const float aspect = w / h;
    
    view = juce::Matrix3D<float>::fromTranslation(cameraPosition);
    projection = juce::Matrix3D<float>::fromFrustum(
              -nearZ * std::tan (fov * 0.5f) * aspect,   // left
               nearZ * std::tan (fov * 0.5f) * aspect,   // right
              -nearZ * std::tan (fov * 0.5f),            // bottom
               nearZ * std::tan (fov * 0.5f),            // top
               nearZ, farZ);
}