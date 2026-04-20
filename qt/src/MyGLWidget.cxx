#include "MyGLWidget.hxx"

void MyGLWidget::initializeGL() 
{
    initializeOpenGLFunctions(); // Required in Qt to access gl* calls

    // These two lines enable 3D depth testing
    glEnable (GL_DEPTH_TEST);
    
    // Enable MSAA
    glEnable (GL_MULTISAMPLE);
    glDepthFunc (GL_LESS);

    // If black screen appears after moving to 4.3, add this to initializeGL
    m_vao.create();
    m_vao.bind();

    // 1. Simple Shaders (Passes texture and coordinates)
    m_program = new QOpenGLShaderProgram (this);

    // Zoom, rotate, drag vertex shader
    // 2D shader
/*
    m_program->addShaderFromSourceCode (QOpenGLShader::Vertex,
                                        "attribute vec2 pos; attribute vec2 tex; \
                                         varying vec2 vTex; \
                                         uniform mat4 mvp; \
                                         void main() { \
                                            vTex = tex; \
                                            gl_Position = mvp * vec4(pos, 0.0, 1.0); \
                                        }"
                                       );
*/

    // 3D shader
/*
    m_program->addShaderFromSourceCode (QOpenGLShader::Vertex,
    "#version 430 core\n"
    "layout(location = 0) in vec3 pos;\n"    // Switched to vec3
    "layout(location = 1) in vec2 tex;\n"
    "out vec2 vTex;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "    vTex = tex;\n"
    "    gl_Position = mvp * vec4(pos, 1.0);\n"
    "}");
*/

    // 3D shader with lighting
    m_program->addShaderFromSourceCode (QOpenGLShader::Vertex,
                                        "#version 430 core\n"
                                        "    layout (location = 0) in vec3 pos;\n"
                                        "    layout (location = 1) in vec2 tex;\n"
                                        "    layout (location = 2) in vec3 normal; // New attribute for light math\n"
                                        "\n"
                                        "    out vec2 vTex;\n"
                                        "    out float vLight; // Pass light intensity to fragment shader\n"
                                        "    uniform mat4 mvp;\n"
                                        "    uniform mat3 normalMatrix; // To rotate normals correctly\n"
                                        "\n"
                                        "    void main() {"
                                        "        vTex = vec2 (tex.x, 1.0 - tex.y);\n"
                                        "        vec3 n = normalize(normalMatrix * normal);\n"
                                        "        // Simple diffuse light from the front-top-right\n"
                                        "        vLight = max (dot (n, normalize ( vec3 (0.5, 1.0, 0.5))), 0.2);\n"
                                        "        gl_Position = mvp * vec4 (pos, 1.0);\n"
                                        "    }"
                                       );


/* Original plain vertex shader
    m_program->addShaderFromSourceCode (QOpenGLShader::Vertex,
                                       "attribute vec2 pos; attribute vec2 tex; \
        varying vec2 vTex; void main() { \
    vTex = tex; gl_Position = vec4(pos, 0.0, 1.0); }");
*/

    // 3D shader
/*
    m_program->addShaderFromSourceCode (QOpenGLShader::Fragment,
                                        "uniform sampler2D sampler; varying vec2 vTex; \
                                         void main() { gl_FragColor = texture2D(sampler, vTex); }"
                                       );
*/

    // 3D shader with lighting
    m_program->addShaderFromSourceCode (QOpenGLShader::Fragment,
                                        "#version 430 core\n"
                                        "    in vec2 vTex;\n"
                                        "    in float vLight;\n"
                                        "    out vec4 fragColor; // Define our own output variable\n"
                                        "    uniform sampler2D sampler;\n"
                                        "    void main() {\n"
                                        "        vec4 texColor = texture(sampler, vTex);\n"
                                        "        fragColor = vec4  (texColor.rgb * vLight, texColor.a);\n"
                                        "    }\n"
                                       );

/*
    m_computeProgram = new QOpenGLShaderProgram (this);
    // Standard GLSL 430 is required for compute shaders
    m_computeProgram->addShaderFromSourceCode (QOpenGLShader::Compute,
                                               "#version 430 core\n"
                                               "layout (local_size_x = 16, local_size_y = 16) in; // 16x16 thread blocks\n"
                                               "layout (rgba8, binding = 0) uniform writeonly image2D outTexture;\n"
                                               "uniform float time;\n"
                                               "void main() {\n"
                                               "    ivec2 texelCoord = ivec2 (gl_GlobalInvocationID.xy);\n"
                                               "    // Dynamic checkerboard based on coordinates and time\n"
                                               "    float val = mod(floor (texelCoord.x / 32.0 + time) + floor (texelCoord.y / 32.0), 2.0);\n"
                                               "    vec4 color = vec4 (val, 0.0, 1.0 - val, 1.0);\n"
                                               "    imageStore (outTexture, texelCoord, color);\n"
                                               "}"
                                              );
*/
    m_program->link();

    if (!m_program->link())
    {
        qDebug() << "Shader Linker Error:" << m_program->log();
    }

    // 2a. Square Geometry (X, Y, U, V)
    //float data[] = { -0.5,-0.5, 0,0,  0.5,-0.5, 1,0,  0.5,0.5, 1,1, -0.5,0.5, 0,1 };
/*
    // 2b. 3D X, Y, Z, U, V
    float data[] =
    { 
        -1.0, -1.0, 0.0,  0.0, 0.0,
         1.0, -1.0, 0.0,  1.0, 0.0,
         1.0,  1.0, 0.0,  1.0, 1.0,
        -1.0,  1.0, 0.0,  0.0, 1.0 
    };
*/
    // 2c. Cude data
    // X, Y, Z, U, V
/*
    float cubeData[] =
    {
        // Front face
        -1.0f, -1.0f,  1.0f, 0.0f, 0.0f,  1.0f, -1.0f,  1.0f, 1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f, 0.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f, 1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 1.0f,
        // Back face
        -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, -1.0f,  1.0f, -1.0f, 1.0f, 1.0f,  1.0f,  1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, -1.0f, 1.0f, 0.0f,  1.0f,  1.0f, -1.0f, 0.0f, 1.0f,  1.0f, -1.0f, -1.0f, 0.0f, 0.0f,
        // Top face
        -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, -1.0f, 0.0f, 1.0f,  1.0f,  1.0f,  1.0f, 1.0f, 0.0f,  1.0f,  1.0f, -1.0f, 1.0f, 1.0f,
        // Bottom face
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,  1.0f, -1.0f, -1.0f, 0.0f, 1.0f,  1.0f, -1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,  1.0f, -1.0f,  1.0f, 0.0f, 0.0f, -1.0f, -1.0f,  1.0f, 1.0f, 0.0f,
        // Right face
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f,  1.0f,  1.0f, -1.0f, 1.0f, 1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 1.0f,
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 0.0f, 1.0f,  1.0f, -1.0f,  1.0f, 0.0f, 0.0f,
        // Left face
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, -1.0f,  1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f,  1.0f,  1.0f, 1.0f, 1.0f, -1.0f,  1.0f, -1.0f, 0.0f, 1.0f
    };
*/

    // X, Y, Z, U, V, NX, NY, NZ (8 floats per vertex)
/*
    float cubeNormalData[] =
    {
        // Front face
        -1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 0,0,1,    1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0,0,1,   1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 0,0,1,
        -1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 0,0,1,    1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 0,0,1,   -1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 0,0,1,

        // Back face
        -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0,0,-1,   -1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 0,0,-1,    1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0,0,-1,
        -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0,0,-1,    1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0,0,-1,    1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0,0,-1,

        // Top face
        -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0,1,0,   -1.0f,  1.0f,  1.0f, 0.0f, 0.0f, 0,1,0,    1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 0,1,0,
        -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0,1,0,    1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 0,1,0,    1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 0,1,0,

        // Bottom face
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0,-1,0,    1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0,-1,0,    1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 0,-1,0,
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0,-1,0,    1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 0,-1,0,   -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0,-1,0,

        // Right face
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 1,0,0,     1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 1,0,0,    1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 1,0,0,
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 1,0,0,     1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 1,0,0,    1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 1,0,0,

        // Left face
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1,0,0,   -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, -1,0,0,   -1.0f,  1.0f,  1.0f, 1.0f, 1.0f, -1,0,0,
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1,0,0,   -1.0f,  1.0f,  1.0f, 1.0f, 1.0f, -1,0,0,   -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, -1,0,0
    };
*/    
    generateSphere (1.5f, 64, 64); // Radius 1.5, 64 sectors/stacks

    m_vbo.create();
    m_vbo.bind();

    // 2D flat data
    //m_vbo.allocate (data, sizeof (data));

    // 3D cube data
//    m_vbo.allocate (cubeData, sizeof (cubeData));

    // 3D cude with normals
//    m_vbo.allocate (cubeNormalData, sizeof (cubeNormalData));

    // 3D sphere
    m_vbo.allocate (m_sphereVertices.data(), m_sphereVertices.size() * sizeof (float));

    // Start a 60 FPS timer to force repaints
/*            QTimer* timer = new QTimer(this);
    connect (timer, &QTimer::timeout, this, QOverload<>::of (&MyGLWidget::update));
    timer->start (16); // ~60 FPS
*/            
    // Generate testing texture
//    textureID = createDynamicTexture (512, 512);

    qDebug() << "Load texture";
    textureID = loadMapTexture ("/data/dev/src/GLGlobe/textures/natural_earth.png");

    if (textureID == 0)
    {
        qDebug() << "Texture ID is 0";
    }

    timer.start();
}

void MyGLWidget::paintGL() 
{
    qDebug() << "MyGLWidget::paintGL()";

    // Compute shader code
    // 1. Run Compute Shader
//    m_computeProgram->bind();
//    m_computeProgram->setUniformValue ("time", (float)timer.elapsed() / 1000.0f);
    
    // Bind texture to Image Unit 0 (matching 'binding = 0' in shader)
    qDebug() << "glBindImageTexture";
    glBindImageTexture (0, textureID, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    
    // Dispatch enough threads to cover a 512x512 texture (512/16 = 32 groups)
//    glDispatchCompute (512 / 16, 512 / 16, 1);
    
    // Ensure compute finishes before the fragment shader tries to read it
    qDebug() << "glMemoryBarrier";
    glMemoryBarrier (GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
//    m_computeProgram->release();

    // End compute shader code

    qDebug() << "glClear";
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 3D persepctive code
    // 1. Projection: 45 degree field of view, aspect aware for 4K
    aspect = (float)width() / (float)(height() > 0 ? height() : 1);
    QMatrix4x4 projection;
    projection.perspective (45.0f, aspect, 0.1f, 100.0f);

    // 2. View/Model: Move camera back 5 units, apply drag/rot
    QMatrix4x4 modelview;
    modelview.translate (m_offset.x(), m_offset.y(), -10.0f); 

    modelview.rotate (m_rotation.x(), 1.0f, 0.0f, 0.0f);
    modelview.rotate (m_rotation.y(), 0.0f, 1.0f, 0.0f);
    modelview.scale (m_zoom);

    // End 3D code

    QMatrix4x4 mvp = projection * modelview;

    if (!m_program->bind()) {qDebug ("ERROR: shader program not bound\n"); return;}

    // Add value for 3D shader
    m_program->setUniformValue ("mvp", mvp);

    // Normal matrix is the inverse transpose of the modelview rotation
    m_program->setUniformValue ("normalMatrix", modelview.normalMatrix());

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
//    m_program->setUniformValue ("mvp", matrix);

    // 4. Draw
    glBindTexture (GL_TEXTURE_2D, textureID);

    // Buffers for no lighting
    // Note: change stride to 5 * sizeof(float) to match new Z coord
    //m_program->setAttributeBuffer (0, GL_FLOAT, 0, 3, 5 * sizeof (float));
    //m_program->setAttributeBuffer (1, GL_FLOAT, 3 * sizeof (float), 2, 5 * sizeof(float));

    // Stride is 8 floats: (3 pos + 2 tex + 3 normal)
    int stride = 8 * sizeof (float);

    //Buffers with lighting
    m_program->setAttributeBuffer (0, GL_FLOAT, 0, 3, stride); // pos
    m_program->setAttributeBuffer (1, GL_FLOAT, 3 * sizeof (float), 2, stride); // tex
    m_program->setAttributeBuffer (2, GL_FLOAT, 5 * sizeof (float), 3, stride); // normal
    m_program->enableAttributeArray (2);

    // Draw call for 2D plane
    //glDrawArrays (GL_TRIANGLE_FAN, 0, 4);

    // Draw call for 3D cube
//    glDrawArrays (GL_TRIANGLES, 0, 36);

    // Draw call for sphere
    glDrawArrays (GL_TRIANGLES, 0, m_sphereVertices.size() / 8);

    // 5. Cleanup
    m_program->disableAttributeArray (posLocation);
    m_program->disableAttributeArray (texLocation);
    m_vao.release();
    m_program->release();

    // FPS Logic
    static int frames = 0;
    static QElapsedTimer fpsTimer;

    if (!fpsTimer.isValid())
    {
        fpsTimer.start();
    }

    frames++;

    if (fpsTimer.elapsed() > 1000)
    {
        // Combine FPS + the camera status we saved earlier
        QString fullStatus = QString ("FPS: %1 | %2")
                             .arg (frames)
                             .arg (m_currentStatusString);
        
        emit cameraChanged(fullStatus);
        
        frames = 0;
        fpsTimer.restart();
    }
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
        m_offset += QVector2D (
                                (diff.x() * aspect * sensitivity) / (float)width(),
                                (-diff.y() * sensitivity) / (float)height()
                              );
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

GLuint MyGLWidget::createDynamicTexture (int w, int h)
{
    GLuint id;
    glGenTextures (1, &id);
    glBindTexture (GL_TEXTURE_2D, id);

    // REQUIRED for compute shaders: Allocate immutable storage
    // We use GL_RGBA8 to match the image2D layout in the shader
    glTexStorage2D (GL_TEXTURE_2D, 1, GL_RGBA8, w, h);

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

void MyGLWidget::generateSphere (float radius, int sectors, int stacks)
{
    std::vector<float> data;
    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / radius;    // vertex normal
    float s, t;                                     // vertex texCoord

    float sectorStep = 2 * M_PI / sectors;
    float stackStep = M_PI / stacks;
    float sectorAngle, stackAngle;

    for (int i = 0; i <= stacks; ++i) 
    {
        stackAngle = M_PI / 2 - i * stackStep;      // starting from pi/2 to -pi/2
        xy = radius * cosf (stackAngle);             // r * cos(u)
        z = radius * sinf (stackAngle);              // r * sin(u)

        for(int j = 0; j <= sectors; ++j)
        {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // Position (x, y, z)
            x = xy * cosf (sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf (sectorAngle);             // r * cos(u) * sin(v)
            data.push_back (x);
            data.push_back (y);
            data.push_back (z);

            // TexCoord (s, t)
            s = (float)j / sectors;
            t = (float)i / stacks;
            data.push_back (s);
            data.push_back (t);

            // Normal (nx, ny, nz)
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            data.push_back (nx);
            data.push_back (ny);
            data.push_back (nz);
        }
    }

    // Generate Indices for Triangles (to use glDrawElements)
    // Or simpler for now: convert to a Triangle List (3 vertices per tri)
    m_sphereVertices.clear();

    for (int i = 0; i < stacks; ++i)
    {
        int k1 = i * (sectors + 1);     // beginning of current stack
        int k2 = k1 + sectors + 1;      // beginning of next stack

        for (int j = 0; j < sectors; ++j, ++k1, ++k2)
        {
            // 2 triangles per sector (except for stacks at poles)
            auto addIdx = [&](int idx)
            {
                int start = idx * 8; // 8 floats per vertex

                for (int n=0; n<8; ++n)
                {
                    m_sphereVertices.push_back (data[start + n]);
                }
            };

            if (i != 0)
            {
                addIdx (k1);
                addIdx (k2);
                addIdx (k1+1);
            } // k1---k1+1---k2

            if (i != (stacks-1))
            {
                addIdx (k1+1);
                addIdx (k2);
                addIdx (k2+1);
            } // k1+1---k2---k2+1
        }
    }
}

GLuint MyGLWidget::loadMapTexture (const QString& filePath)
{
    qDebug() << "Loading image: " << filePath;

    QImageReader reader (filePath);
    
    // Bypass the default 128MB limit for your 8k texture
    reader.setAllocationLimit (1024); 

    if (!reader.canRead())
    {
        qDebug() << "Cannot read image: " << reader.errorString();
        return 0;
    }

    qDebug() << "Reading image: " << filePath;

    // Optional: Downscale during load to stay within ROCm memory stability limits
    if (reader.size().width() > 4096)
    {
        reader.setScaledSize (QSize (4096, 2048));
        qDebug() << "Scaling image:" << filePath;
    }

    qDebug() << "Reading image: " << filePath;

    QImage img = reader.read();

    if (img.isNull())
    {
        qDebug() << "Load failed: " << reader.errorString();
        return 0;
    }

    // Convert to RGBA8888 for GL_RGBA8 compatibility
    // Use flipped() to move the origin from top-left to bottom-left for OpenGL

    qDebug() << "Converting image: " << filePath;
    img = img.convertToFormat (QImage::Format_RGBA8888).flipped();

    qDebug() << "Initializing texture: " << filePath;
    GLuint textureID;
    glGenTextures (1, &textureID);
    glBindTexture (GL_TEXTURE_2D, textureID);

    qDebug() << "Setting parameters for globe: " << filePath;
    // Texture parameters for the globe
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    qDebug() << "Uploading to GPU: " << filePath;
    // Upload to the RX 7800XT
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, 
                 img.width(), img.height(), 0, 
                 GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());

    qDebug() << "Generating MIPMAP: " << filePath;
    glGenerateMipmap (GL_TEXTURE_2D);

    qDebug() << "Uplaod complete: " << filePath;
    return textureID;
}
