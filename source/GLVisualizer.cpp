#include "GLVisualizer.h"
#include "MainController.h"

//=============================================================================
juce::Matrix3D<float> makeOrthoScale(float sx, float sy)
{
    juce::Matrix3D<float> m;
    m.mat[0]  = sx;   // column-major: scale X
    m.mat[5]  = sy;   // scale Y  (mat[5] is row1,col1)
    m.mat[10] = 1.0f; // scale Z
    m.mat[15] = 1.0f; // homogeneous
    return m;
}

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
        in  vec2 position;
        out vec2 vPos;
        out float vDepth;

        uniform mat4 uMVP;
        uniform float uCurrentZ;
        uniform float uFadeEndZ;

        void main()
        {
            // Quad vertex in object space (z = 0)
            vec4 worldPos = vec4 (position, 0.0, 1.0);

            // Standard transform
            vec4 eyePos = uMVP * worldPos;
            gl_Position = eyePos;

            // Supply helpers to fragment stage
            vPos = position * 10.0;
            vDepth = -uCurrentZ / uFadeEndZ;
            // vDepth = 0.5;
        }
    )";

    // GLSL fragment shader initialization code
    static const char* fragSrc = R"(#version 150
        in  vec2  vPos;
        in  float vDepth;
        out vec4  frag;

        void main()
        {
            // circular mask
            if (dot(vPos, vPos) > 1.0)
                discard;

            float alpha = 1.0 - vDepth;      // fade out with distance
            frag = vec4(1.0, 1.0, 1.0, alpha);
        }
    )";
    
    // Compile and link the shaders
    shader.reset(new juce::OpenGLShaderProgram(openGLContext));
    shader->addVertexShader(vertSrc);
    shader->addFragmentShader(fragSrc);

    ext.glBindAttribLocation(shader->getProgramID(), 0, "position");

    bool shaderLinked = shader->link();
    jassert(shaderLinked);
    juce::ignoreUnused(shaderLinked);

    // Build a 2×2 clip-space quad centred at (0,0)
    vbo.create(openGLContext);
    ext.glBindBuffer(GL_ARRAY_BUFFER, vbo.id);

    const float quad[8] = { -0.1f, -0.1f,
                             0.1f, -0.1f,
                            -0.1f,  0.1f,
                             0.1f,  0.1f };
    ext.glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    // Generate and bind the vertex-array object
    ext.glGenVertexArrays(1, &vao);
    ext.glBindVertexArray(vao);

    // Tell the VAO about our single attribute once
    ext.glBindBuffer(GL_ARRAY_BUFFER, vbo.id);
    ext.glEnableVertexAttribArray(0);
    ext.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Force repaint to make sure the mvp is correct
    resized();
}

void GLVisualizer::shutdown()
{
    auto& ext = openGLContext.extensions;
    if (vao != 0) { ext.glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo.id != 0) { ext.glDeleteBuffers(1, &vbo.id); vbo.id = 0; }
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

    if (shader == nullptr)
        return;         // early-out on link failure

    shader->use();

    shader->setUniform ("uFadeEndZ", fadeEndZ); 

    // Compute position for the new particle
    float t = (float)(juce::Time::getMillisecondCounterHiRes() 
                      * 0.001 - startTime);
    float r = 0.75f;     
    float w = juce::MathConstants<float>::twoPi * 0.4f;
    float x = r * std::cos(w * t);
    float y = r * std::sin(w * t);

    // Add new particle to the queue
    Particle newParticle = { x, y, t };
    particles.push_back(newParticle);

    // Delete old particles
    while (!particles.empty())
    {
        const float age = t - particles.front().spawnTime;
        if (age * speed < fadeEndZ)   // still visible?
            break;                    // oldest one is still alive
        particles.pop_front();        // otherwise discard and test next
    }

    // Draw all particles!!
    for (const auto& p : particles)
    {
        // Calculate z value
        const float age = t - p.spawnTime; // seconds since spawn
        const float z = -age * speed;
        shader->setUniform ("uCurrentZ", z);

        // Set model matrix and mvp for this particle
        juce::Vector3D<float> position { p.spawnX, p.spawnY, z };
        model = juce::Matrix3D<float>::fromTranslation(position);
        mvp = projection * view * model;
        shader->setUniformMat4("uMVP", mvp.mat, 1, GL_FALSE);

        // Bind vertex array and draw
        ext.glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

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