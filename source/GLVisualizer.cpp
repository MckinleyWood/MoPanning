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
    openGLContext.setOpenGLVersionRequired (
        juce::OpenGLContext::OpenGLVersion::openGL3_2);
    openGLContext.setContinuousRepainting(true);
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

        void main()
        {
            vPos = position * 10.0;
            gl_Position = uMVP * vec4(position, 0.0, 1.0);
        }
    )";

    // GLSL fragment shader initialization code
    static const char* fragSrc = R"(#version 150
        in  vec2 vPos;
        out vec4 frag;

        void main()
        {
            if (dot (vPos, vPos) > 1.0)         // outside unit circle?
                discard;
            frag = vec4(1.0, 1.0, 1.0, 1.0);       // opaque white
        }
    )";
    
    // Compile and link the shaders
    shader.reset(new juce::OpenGLShaderProgram(openGLContext));
    shader->addVertexShader(vertSrc);
    shader->addFragmentShader(fragSrc);
    shader->link();

    // Build a 2×2 clip-space quad centred at (0,0)
    vbo.create(openGLContext);
    ext.glBindBuffer(GL_ARRAY_BUFFER, vbo.id);

    const float quad[8] = {  -0.1f, -0.1f,
                              0.1f, -0.1f,
                             -0.1f,  0.1f,
                              0.1f,  0.1f };
    ext.glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    // Generate and bind the vertex-array object
    ext.glGenVertexArrays(1, &vao);
    ext.glBindVertexArray(vao);

    // Tell the VAO about our single attribute once
    ext.glBindBuffer (GL_ARRAY_BUFFER, vbo.id);
    ext.glEnableVertexAttribArray (0);
    ext.glVertexAttribPointer     (0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Force repaint to make sure the mvp is correct
    resized();
}

void GLVisualizer::shutdown()
{
    auto& ext = openGLContext.extensions;
    if (vao != 0) { ext.glDeleteVertexArrays (1, &vao); vao = 0; }
    if (vbo.id != 0) { ext.glDeleteBuffers (1, &vbo.id); vbo.id = 0; }
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

    ext.glBindBuffer(GL_ARRAY_BUFFER, vbo.id);
    // ext.glEnableVertexAttribArray(0);
    // ext.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    ext.glBindVertexArray(vao);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // jassert(juce::gl::glGetError() == GL_NO_ERROR);

    // ext.glDisableVertexAttribArray(0);
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