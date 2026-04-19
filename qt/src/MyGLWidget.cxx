#include "MyGLWidget.hxx"

void MyGLWidget::initializeGL() 
{
    initializeOpenGLFunctions(); // Required in Qt to access gl* calls

    // If black screen appears after moving to 4.3, add this to initializeGL
    m_vao.create();
    m_vao.bind(); 

    // 1. Simple Shaders (Passes texture and coordinates)
    m_program = new QOpenGLShaderProgram (this);

    // Zoom, rotate, drag vertex shader
    m_program->addShaderFromSourceCode (QOpenGLShader::Vertex,
                                        "attribute vec2 pos; attribute vec2 tex; \
                                         varying vec2 vTex; \
                                         uniform mat4 mvp; \
                                         void main() { \
                                            vTex = tex; \
                                            gl_Position = mvp * vec4(pos, 0.0, 1.0); \
                                        }"
                                       );

/* Original plain vertex shader
    m_program->addShaderFromSourceCode (QOpenGLShader::Vertex,
                                       "attribute vec2 pos; attribute vec2 tex; \
        varying vec2 vTex; void main() { \
    vTex = tex; gl_Position = vec4(pos, 0.0, 1.0); }");
*/

    m_program->addShaderFromSourceCode (QOpenGLShader::Fragment,
                                        "uniform sampler2D sampler; varying vec2 vTex; \
                                         void main() { gl_FragColor = texture2D(sampler, vTex); }"
                                       );

    if (!m_program->link())
    {
        qDebug() << "Shader Linker Error:" << m_program->log();
    }

    // 2. Square Geometry (X, Y, U, V)
    float data[] = { -0.5,-0.5, 0,0,  0.5,-0.5, 1,0,  0.5,0.5, 1,1, -0.5,0.5, 0,1 };
    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate (data, sizeof (data));

    // Start a 60 FPS timer to force repaints
/*            QTimer* timer = new QTimer(this);
    connect (timer, &QTimer::timeout, this, QOverload<>::of (&MyGLWidget::update));
    timer->start (16); // ~60 FPS
*/            
    // Generate testing texture
    textureID = createSimpleTexture (512, 512);
}

void MyGLWidget::paintGL() 
{
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_program->bind();
    m_vao.bind();  // Must be bound during the draw call
    m_vbo.bind();  // Must be bound to link attributes

    // 1. Link "pos" (attribute index 0)
    // data is: [X, Y, U, V] -> 4 floats total
    int posLocation = m_program->attributeLocation ("pos");
    m_program->enableAttributeArray (posLocation);
    m_program->setAttributeBuffer (posLocation, GL_FLOAT, 0, 2, 4 * sizeof(float));

    // 2. Link "tex" (attribute index 1)
    // Starts after 2 floats (X,Y)
    int texLocation = m_program->attributeLocation ("tex");
    m_program->enableAttributeArray (texLocation);
    m_program->setAttributeBuffer (texLocation, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));

    // 3. Set Uniforms
    QMatrix4x4 matrix;
    matrix.translate (m_offset.x(), m_offset.y(), 0.0f);
    matrix.rotate (m_rotation.x(), 1.0f, 0.0f, 0.0f);
    matrix.rotate (m_rotation.y(), 0.0f, 1.0f, 0.0f);
    matrix.scale (m_zoom);
    m_program->setUniformValue ("mvp", matrix);

    // 4. Draw
    glBindTexture (GL_TEXTURE_2D, textureID);
    glDrawArrays (GL_TRIANGLE_FAN, 0, 4);

    // 5. Cleanup
    m_program->disableAttributeArray (posLocation);
    m_program->disableAttributeArray (texLocation);
    m_vao.release();
    m_program->release();
}

void MyGLWidget::resizeGL (int w, int h)
{
    glViewport (0, 0, w, h);
    updateStatus(); // Update the UI with new aspect-aware pos
}


// Input Handlers

// Generic mouse handlers
/*
void MyGLWidget::mousePressEvent (QMouseEvent *event) override
{
    // Example: print click coordinates
    qDebug() << "Mouse clicked at:" << event->position();
    update(); // Triggers a repaint
}

 End generic mouse handlers */

void MyGLWidget::keyPressEvent (QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        close();
    }

    updateStatus();
}


// Transform mouse handlers

void MyGLWidget::wheelEvent (QWheelEvent *event)
{
    float delta = event->angleDelta().y() > 0 ? 1.1f : 0.9f;
    m_zoom *= delta;
    updateStatus();
}

void MyGLWidget::mouseMoveEvent (QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        QPoint diff = event->pos() - m_lastMousePos;
        // Dragging logic (adjust sensitivity as needed)
        m_offset += QVector2D (diff.x() / (float)width(), -diff.y() / (float)height()) * 2.0f;
    }
    else if (event->buttons() & Qt::RightButton)
    {
        QPoint diff = event->pos() - m_lastMousePos;
        // Rotation logic
        m_rotation += QVector2D (diff.y(), diff.x());
    }

    m_lastMousePos = event->pos();
    updateStatus();
}

void MyGLWidget::mousePressEvent (QMouseEvent *event)
{
    m_lastMousePos = event->pos();
}


GLuint MyGLWidget::createSimpleTexture (int w, int h)
{
    GLuint id;
    glGenTextures (1, &id);
    glBindTexture (GL_TEXTURE_2D, id);

    std::vector<unsigned char> px (w * h * 4);

    for (int i=0; i<w*h; ++i)
    {
        int x = i % w, y = i / w;
        unsigned char c = ((x/32 + y/32) % 2 == 0) ? 255 : 100;
        px[i*4]=c; px[i*4+1]=0; px[i*4+2]=255-c; px[i*4+3]=255;
    }

    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return id;
}

// Inside your widget for executing compute shader
void MyGLWidget::runCompute()
{
//            glUseProgram (computeShaderProgramID);
//            glDispatchCompute (groups_x, groups_y, groups_z);
//            glMemoryBarrier (GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
