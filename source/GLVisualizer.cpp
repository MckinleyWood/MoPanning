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
        in vec2 position;
        out vec2 vPos;
        uniform mat4 uMVP;
        uniform vec2 uOffset; 
        uniform float uAge;
        uniform float uMaxAge;

        void main()
        {
            float depth = -uAge / uMaxAge;
            vec2 p2D = position + uOffset; 
            vPos = position * 10.0;
            gl_Position = uMVP * vec4(p2D, depth, 1.0);
        }
    )";

    // GLSL fragment shader initialization code
    static const char* fragSrc = R"(#version 150
        in  vec2 vPos;
        out vec4 frag;
        uniform float uAge;
        uniform float uMaxAge;

        void main()
        {
            if (dot(vPos, vPos) > 1.0)         // outside unit circle?
                discard;
            float alpha = 1.0 - clamp(uAge / uMaxAge, 0.0, 1.0);
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

    // Clear to black
    juce::OpenGLHelpers::clear(juce::Colours::black);

    if (shader == nullptr)
        return;         // early-out on link failure

    shader->use();
    shader->setUniformMat4("uMVP", mvp.mat, 1, GL_FALSE);

    juce::OpenGLShaderProgram::Uniform uOffset(*shader, "uOffset");
    juce::OpenGLShaderProgram::Uniform uAge   (*shader, "uAge");
    juce::OpenGLShaderProgram::Uniform uMaxAge(*shader, "uMaxAge");

    // Compute new uOffset to make the circle do its orbit
    double t = juce::Time::getMillisecondCounterHiRes() * 0.001 - startTime;
    float r = 0.75f;     
    float w = juce::MathConstants<float>::twoPi * 0.5f; // 0.2 rev/s
    float x = r * std::cos(w * (float)t);
    float y = r * std::sin(w * (float)t);
    
    // Update uniforms
    uOffset.set(x, y);
    uAge.set((float)std::fmod(t, maxAge));
    DBG("uAge = " << (float)std::fmod(t, maxAge));
    uMaxAge.set(maxAge);
    DBG("uMaxAge = " << maxAge);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Bind VBO and VAO and draw
    ext.glBindBuffer(GL_ARRAY_BUFFER, vbo.id);
    ext.glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    jassert(juce::gl::glGetError() == GL_NO_ERROR);
}

void GLVisualizer::resized() 
{
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    // Orthographic matrix that keeps –1..+1 regardless of aspect
    auto aspect = w / h;
    if (aspect >= 1.0f)
        mvp = makeOrthoScale(1.0f / aspect, 1.0f);
    else
        mvp = makeOrthoScale(1.0f, aspect);
}