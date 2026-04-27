#include "MyGLWidget.hxx"
#include "MainWindow.hxx"

using namespace SimCore;

void MyGLWidget::initializeGL() 
{
    initCapitals ("/home/pgallen/Downloads/capitals.csv");
    initSatellites();

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

//    registerShader ("Standard", "shaders/Earth.vert", "shaders/Earth.frag");
    registerShader ("NightLights", "shaders/Earth.vert", "shaders/Earth-night.frag");
    registerShader ("BumpLights", "shaders/Earth-Bump.vert", "shaders/Earth-Bump.frag");

    // Set the default
    m_program = Globe::m_shaders["BumpLights"];

    if (!m_program->link())
    {
        qDebug() << "Shader Linker Error:" << m_program->log();
    }

    generateSphere (Globe::globeRadius, Globe::globeSectors, Globe::globeStacks);

    m_vbo.create();
    m_vbo.bind();

    // 3D sphere
    m_vbo.allocate (m_sphereVertices.data(), m_sphereVertices.size() * sizeof (float));

    qDebug() << "Load texture";
    //textureID = loadTexture (mapSizes.huge, "textures/1_earth_16k.jpg");
    
    if (!loadTextureFiles (Globe::mapSizes.huge))
    {
        qCritical() << "FATAL ERROR: Failed to initialize textures";
        QCoreApplication::exit (1); // Exit app
        return;
    }

    dayTextureID =   Globe::textureMap["earthncice16k"];
    nightTextureID = Globe::textureMap["earthnight16k"];
    bumpTextureID =  Globe::textureMap["earthbump16k"];
    //textureID = textureMap["earth16k"];

    std::cout << "Texture ID is " << textureID << std::endl;

    initializeGlobePosition();

    Globe::timer.start();
}

void MyGLWidget::paintGL() 
{
    // Compute shader code
//    m_computeProgram->bind();
//    m_computeProgram->setUniformValue ("time", (float)timer.elapsed() / 1000.0f);
    
    // Bind texture to Image Unit 0 (matching 'binding = 0' in shader)
//    glBindImageTexture (0, textureID, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    
    // Dispatch enough threads to cover a 512x512 texture (512/16 = 32 groups)
//    glDispatchCompute (512 / 16, 512 / 16, 1);
    
    // Ensure compute finishes before the fragment shader tries to read it
//    glMemoryBarrier (GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
//    m_computeProgram->release();

    // End compute shader code


    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable (GL_DEPTH_TEST);

    // Projection (The 4K Lens)
    float aspect = (float)width() / (float)height();
    QMatrix4x4 projection;
    projection.perspective (45.0f, aspect, 0.1f, 100.0f);

    // View (The Camera/Mouse controls)
    QMatrix4x4 view;
    view.translate (m_offset.x(), m_offset.y(), -10.0f * m_zoom);
    // These rotations let the mouse "orbit" the globe
    view.rotate (m_rotation.x(), 1.0f, 0.0f, 0.0f);
    view.rotate (m_rotation.y(), 0.0f, 1.0f, 0.0f);

    QMatrix4x4 model;

    // Axial Tilt: Use a NEGATIVE rotation to tilt the North Pole TOWARD the sun in April
    model.rotate (Globe::m_liveTilt, 1.0f, 0.0f, 0.0f); 

    // Real-Time Spin:
    // We use UTC time to avoid local daylight savings confusion
    qint64 msecs = QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay();
    float dayFraction = (float)msecs / 86400000.0f;
    
    // Offset calculation: 
    // -90 aligns 0-longitude with 'noon' at 12:00 UTC
    float spinAngle = (dayFraction * 360.0f) + Globe::m_liveOffset;
    
    model.rotate (spinAngle, 0.0f, 1.0f, 0.0f);

    QMatrix4x4 mvp = projection * view * model;
    QMatrix4x4 modelView = view * model; // Capture this for label culling

    // Save matrices for reference
    Globe::modelMatrix = model;
    Globe::viewMatrix = view;
    Globe::projectMatrix = projection;
   
    // 4. Update Uniforms
    m_program->bind();
    m_program->setUniformValue ("ambientIntensity", (float)Globe::m_ambientLevel);
    m_program->setUniformValue ("modelMatrix", model);
    m_program->setUniformValue ("sunDirection", QVector3D (0, 0, 1));
    m_program->setUniformValue ("mvp", mvp);

    // Bind Day Texture to Unit 0
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, dayTextureID);
    m_program->setUniformValue ("daySampler", 0);

    // Bind Night Texture to Unit 1
    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_2D, nightTextureID);
    m_program->setUniformValue ("nightSampler", 1);

    // Bind the Bump/Height Map
    glActiveTexture (GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, bumpTextureID);
    m_program->setUniformValue("bumpSampler", 2);

    // 3. Drawing
    m_vao.bind();
    m_vbo.bind();
    int stride = 8 * sizeof (float);

    m_program->enableAttributeArray (0);
    m_program->setAttributeBuffer (0, GL_FLOAT, 0, 3, stride); // pos
    m_program->enableAttributeArray (1);
    m_program->setAttributeBuffer (1, GL_FLOAT, 3 * sizeof (float), 2, stride); // tex
    m_program->enableAttributeArray (2);
    m_program->setAttributeBuffer (2, GL_FLOAT, 5 * sizeof (float), 3, stride); // normal

    glDrawArrays (GL_TRIANGLES, 0, m_sphereVertices.size() / 8);

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
        
        emit cameraChanged (fullStatus);
        
        frames = 0;
        fpsTimer.restart();
    }
    

        /********************** 2D Painter *********************/
        glDisable (GL_DEPTH_TEST);
        glDisable (GL_CULL_FACE);
        QPainter painter (this);

    //    painter.beginNativePainting();

        painter.setRenderHint (QPainter::Antialiasing);
        QRect viewport (0, 0, width(), height());

    /*************** City labels ***************/
    if (Globe::m_showCities)
    {

        // Paint test (a large point on the North Pole, always visible
    /*
        // 2. Use the exact matrices from your globe draw
        QVector3D northPole (0.0f, 1.51f, 0.0f); // North Pole is Y-up

        // 3. Manual Projection to bypass 'project()' bugs
        QVector4D clipPos = mvp * QVector4D (northPole, 1.0f);

        if (clipPos.w() != 0.0f) {
            // Convert to Normalized Device Coordinates (-1 to 1)
            float ndcX = clipPos.x() / clipPos.w();
            float ndcY = clipPos.y() / clipPos.w();
            float ndcZ = clipPos.z() / clipPos.w();

            // Only draw if it's within the view frustum (Z is -1 to 1 in NDC)
            if (ndcZ >= -1.0f && ndcZ <= 1.0f) {
                // Convert NDC to Pixel Coordinates
                int x = (int)((ndcX + 1.0f) * 0.5f * width());
                int y = (int)((1.0f - ndcY) * 0.5f * height());

                // Draw a giant marker to confirm it exists
                painter.setBrush(Qt::green);
                painter.setPen(QPen(Qt::white, 4));
                painter.drawEllipse(QPoint(x, y), 20, 20);
                
                painter.setFont(QFont("Arial", 16, QFont::Bold));
                painter.drawText(x + 25, y, "NP");
            }
        }
    */
        for (const auto& city : Globe::m_capitals)
        {
            QVector3D worldPos = Utility::latLonToXYZ (Globe::m_liveOffset, city.lat, city.lon, Globe::cityLabelHeight);
            QVector4D clipPos = mvp * QVector4D (worldPos, 1.0f);

            // Set up the Pen (for the outline and text)
            QPen myPen (Globe::m_textColor);
            myPen.setWidth (1); 

            // Set up the Brush (for the fill of the circle)
            QBrush myBrush (Globe::m_cityColor);
            painter.setBrush (myBrush);

            // Set up the shadow pen
            QPen shadowPen (Globe::m_shadowColor);
            shadowPen.setWidth (7);

            // Create a font object with your preferred family
            QFont cityFont ("Arial", Globe::m_fontSize, QFont::Normal);
            painter.setFont (cityFont);
            
            if (clipPos.w() != 0.0f)
            {
                float ndcX = clipPos.x() / clipPos.w();
                float ndcY = clipPos.y() / clipPos.w();
                float ndcZ = clipPos.z() / clipPos.w();

                // 1. Frustum Check (Is it in view?)
                // 2. Depth Check (Is it on the front side? ndcZ < 0.5 is a safe bet here)
                if (ndcZ >= -1.0f && ndcZ <= 1.0f)
                {
                    // Front-side check: Transform to View Space to check Z
                    if ((view * model).map (worldPos).z() > (view * model).map (QVector3D (0, 0, 0)).z())
                    {
                        int x = (int)((ndcX + 1.0f) * 0.5f * width());
                        int y = (int)((1.0f - ndcY) * 0.5f * height());

    //                    painter.setBrush (Qt::cyan);
    //                    painter.setPen (QPen (Qt::black, 1));

                        painter.drawEllipse (QPointF (x, y), Globe::m_markerSize, Globe::m_markerSize);

                        if (Globe::m_selectedCity)
                        {
                            QVector3D worldPos = Utility::latLonToXYZ (Globe::m_liveOffset,
                                                                       Globe::m_selectedCity->lat,
                                                                       Globe::m_selectedCity->lon,
                                                                       Globe::cityLabelHeight
                                                                      );
                            QVector4D clipPos = mvp * QVector4D (worldPos, 1.0f);
                            
                            // Convert to pixel space
                            int x = (int)((clipPos.x() / clipPos.w() + 1.0f) * 0.5f * width());
                            int y = (int)((1.0f - clipPos.y() / clipPos.w()) * 0.5f * height());

                            // Info Card Styling
                            int cardW = 200;
                            int cardH = 80;
                            QRect cardRect (x + 20, y - 40, cardW, cardH);

                            // Draw Background with Transparency
                            painter.setBrush (QColor (0, 0, 0, 180)); // Semi-transparent black
                            painter.setPen (QPen (Qt::cyan, 2));
                            painter.drawRoundedRect (cardRect, 10, 10);

                            // Draw Content
                            painter.setPen (Qt::white);
                            painter.setFont (QFont ("Arial", Globe::m_fontSize, QFont::Bold));
                            painter.drawText (cardRect.adjusted (10, 10, -10, -10), Qt::AlignTop, Globe::m_selectedCity->name);
                            
                            painter.setFont (QFont ("Arial", Globe::m_fontSize));
                            painter.drawText (cardRect.adjusted (10, 35, -10, -10), Qt::AlignTop, Globe::m_selectedCity->extraInfo);
                        }
                        else
                        {
                            painter.setPen (shadowPen);
                            painter.drawText (x + 5, y + 5, city.name);

                            painter.setPen (myPen);
                            painter.drawText (x + 10, y, city.name);
                        }
                    }
                } // if (ndcZ >= -1.0f && ndcZ <= 1.0f)
            } // if (clipPos.w() != 0.0f)
        } // for (const auto& city : m_capitals)
    } // if (m_showCities)

    // Satellites
    if (!m_satTimer.isValid() || m_satTimer.elapsed() > 100)
    {
        qint64 currentMsecs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
        updateSatellitePhysics (currentMsecs); // Update position for this frame
        m_lastIssPos = m_issPos; // Store the stable position
        m_satTimer.restart();
    }

    QVector4D clipPos = mvp * QVector4D (m_lastIssPos, 1.0f);

    if (std::abs (clipPos.w()) > 0.001f)
    { // Use a safer epsilon check
        float ndcX = clipPos.x() / clipPos.w();
        float ndcY = clipPos.y() / clipPos.w();

        int x = (int)((ndcX + 1.0f) * 0.5f * width());
        int y = (int)((1.0f - ndcY) * 0.5f * height());

        painter.setBrush (Qt::magenta); // Bright magenta for visibility
        painter.drawEllipse (QPoint (x, y), 6, 6);
        painter.drawText (x + 10, y, "ISS");
    }

//    painter.endNativePainting(); 

    painter.end();

    glEnable (GL_DEPTH_TEST);

    updateStatus();
} // MyGLWidget::paintGL() 

void MyGLWidget::resizeGL (int w, int h)
{
    glViewport (0, 0, w, h);
    updateStatus(); // Update the UI with new aspect-aware pos
}

// Inside your widget for executing compute shader
void MyGLWidget::runCompute()
{
//            glUseProgram (computeShaderProgramID);
//            glDispatchCompute (groups_x, groups_y, groups_z);
//            glMemoryBarrier (GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

// Input Handlers

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

    if (m_zoom < ZOOM_CLAMP)
    {
        m_zoom = ZOOM_CLAMP;
    }

    updateStatus();
}

void MyGLWidget::mouseMoveEvent (QMouseEvent *event)
{
    float adaptiveSens = sensitivity * m_zoom; 
    float currentAspect = (float)width() / (float)height();

    if (event->buttons() & Qt::LeftButton)
    {
        QPoint diff = event->pos() - m_lastMousePos;
        // Dragging logic (adjust sensitivity as needed)
        m_offset += QVector2D (
                                (diff.x() * currentAspect * adaptiveSens) / (float)width(),
                                (-diff.y() * adaptiveSens) / (float)height()
                              );
    }
    else if (event->buttons() & Qt::RightButton)
    {
        QPoint diff = event->pos() - m_lastMousePos;
        // Rotation logic
        adaptiveSens = rotSensitivity * m_zoom;
        m_rotation += QVector2D (diff.y() * adaptiveSens, diff.x() * adaptiveSens);
    }

    m_lastMousePos = event->pos();
    updateStatus();
}

void MyGLWidget::mousePressEvent (QMouseEvent *event)
{
    QString log;

    // Save for dragging, rotating
    m_lastMousePos = event->pos();

    if (event->button() & Qt::LeftButton)
    {
        // --- Picking Logic ---
        QMatrix4x4 mvp = Globe::projectMatrix * Globe::viewMatrix * Globe::modelMatrix;
        QMatrix4x4 modelView = Globe::viewMatrix * Globe::modelMatrix;
        float width = (float)this->width();
        float height = (float)this->height();

        for (const auto& city : Globe::m_capitals)
        {
            QVector3D worldPos = Utility::latLonToXYZ (Globe::m_liveOffset, city.lat, city.lon, Globe::cityLabelHeight);
            
            // 1. Only check cities on the front side
            if (modelView.map (worldPos).z() > modelView.map (QVector3D (0, 0, 0)).z())
            {                
                // 3. Project to NDC (-1 to 1)
                QVector4D clipPos = mvp * QVector4D (worldPos, 1.0f);

                if (clipPos.w() != 0.0f)
                {
                    float ndcX = clipPos.x() / clipPos.w();
                    float ndcY = clipPos.y() / clipPos.w();
                    
                    // 4. Convert to Pixel Space (Same as your working NP code)
                    float pixelX = (ndcX + 1.0f) * 0.5f * width;
                    float pixelY = (1.0f - ndcY) * 0.5f * height;

                    // 5. Check distance against Mouse Position
                    // Use event->position() for Qt 6 high-DPI accuracy
                    QVector2D cityPixel (pixelX, pixelY);
                    float dist = (cityPixel - QVector2D (event->position())).length();

                    if (dist < 10.0f)
                    {
                        Globe::m_selectedCity = &city; // Store reference
                        MainWindow::instance()->logMessage ("Selected: " + city.name);
                        update(); // Force redraw for the info card
                        return;
                    }

                    Globe::m_selectedCity = nullptr; // Clear if no city clicked
                }
            }
        }
    }
    
    update();
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

float* MyGLWidget::createPlane()
{
    static float data[] = {
                    -0.5, -0.5,  0, 0, 
                    0.5,  -0.5,  1, 0, 
                    0.5,   0.5,  1, 1,
                    -0.5,  0.5,  0, 1
                   };

    return data;
}

float* MyGLWidget::createLargePlane()
{
    static float data[] =
    { 
        -1.0, -1.0, 0.0,  0.0, 0.0,
         1.0, -1.0, 0.0,  1.0, 0.0,
         1.0,  1.0, 0.0,  1.0, 1.0,
        -1.0,  1.0, 0.0,  0.0, 1.0 
    };

    return data;
}

float* MyGLWidget::createNormalCube()
{
    static float cubeNormalData[] =
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

    return cubeNormalData;
}

float* MyGLWidget::createCube()
{
    static float cubeData[] =
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

    return cubeData;
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
        y = radius * sinf (stackAngle);              // r * sin(u)

        for (int j = 0; j <= sectors; ++j)
        {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // Position (x, y, z)
            x = xy * cosf (sectorAngle);             // r * cos(u) * cos(v)
            z = xy * sinf (sectorAngle);             // r * cos(u) * sin(v)
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
                addIdx (k1 + 1);
            } // k1---k1+1---k2

            if (i != (stacks-1))
            {
                addIdx (k1 + 1);
                addIdx (k2);
                addIdx (k2 + 1);
            } // k1+1---k2---k2+1
        }
    }
}

QVector3D MyGLWidget::calculateSunDirection()
{
    // 1. Get Day of Year for Seasonal Tilt (North/South light balance)
    int dayOfYear = QDate::currentDate().dayOfYear();
    // Earth is tilted 23.44 degrees. This formula finds the sun's relative latitude.
    float solarDeclination = Globe::m_liveTilt * sinf ((2.0f * M_PI / 365.0f) * (dayOfYear - 81));

    // 2. Calculate the Sun Vector
    QMatrix4x4 sunTransform;
    
    // Season: Tilt the light source Up/Down based on the date
    sunTransform.rotate (solarDeclination, 1.0f, 0.0f, 0.0f);
    
    // Time of Day: The Sun's longitude (0 longitude is noon)
    float msecs = QTime::currentTime().msecsSinceStartOfDay();
    float dayFraction = msecs / 86400000.0f;
    float solarLongitude = (dayFraction * 360.0f) + Globe::m_liveOffset;// + 180.0f;
    
    sunTransform.rotate (solarLongitude, 0.0f, 1.0f, 0.0f);

    // Return the direction from the Sun to the Earth (fixed in World Space)
    //return sunTransform.map (QVector3D (0, 0, 1)).normalized();
    
    return QVector3D (0.0f, 0.0f, 1.0f);
}

void MyGLWidget::initializeGlobePosition()
{
    m_zoom = 1.0f;
    m_offset = QVector2D (0.0f, 0.0f);
    Globe::m_liveOffset = -90.0f;
    Globe::m_liveTilt = 23.5f;
    Globe::m_ambientLevel = 0.15f;

    MainWindow::instance()->logMessage ("Globe reset to default position");
}


GLuint MyGLWidget::loadTexture (std::array<int, 2>& mapSize, const QString& filePath)
{
    QImageReader reader (filePath);
    
    // Bypass the default 128MB limit for your 8k texture
    reader.setAllocationLimit (1024); 

    if (!reader.canRead())
    {
        qDebug() << "Cannot read image: " << reader.errorString();
        return 0;
    }

    // Optional: Downscale during load to stay within ROCm memory stability limits
    //if (reader.size().width() > 8192)
    {
        reader.setScaledSize (QSize (mapSize[0], mapSize[1]));
    }

    QImage img = reader.read();

    if (img.isNull())
    {
        std::cout << "Load failed: " << reader.errorString().toStdString().c_str() << std::endl;
        return 0;
    }

    // Convert to RGBA8888 for GL_RGBA8 compatibility
    // Use flipped() to move the origin from top-left to bottom-left for OpenGL
    img = img.convertToFormat (QImage::Format_RGBA8888).flipped (Qt::Horizontal);

    GLuint textureID;
    glGenTextures (1, &textureID);
    glBindTexture (GL_TEXTURE_2D, textureID);

    // Texture parameters for the globe
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload to the RX 7800XT
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, 
                 img.width(), img.height(), 0, 
                 GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());

    glGenerateMipmap (GL_TEXTURE_2D);

    return textureID;
}

bool MyGLWidget::loadTextureFiles (std::array<int, 2>& mapSize)
{
    for (const auto& pair : Globe::TextureFiles)
    {
        GLuint textureID = loadTexture (mapSize, pair.second);
        
        if (textureID > 0)
        {
            Globe::textureMap.insert ({pair.first, textureID});
        }
        else
        {
            std::cout << "Fatal error: Texure ID is 0" << std::endl; 
            return false;
        }
    }

    return true;
}


bool MyGLWidget::initShader (QOpenGLShaderProgram* program, const QString& vPath, const QString& fPath)
{
    program->removeAllShaders();
    
    if (!program->addShaderFromSourceFile (QOpenGLShader::Vertex, vPath))
        return false;
    
    if (!program->addShaderFromSourceFile (QOpenGLShader::Fragment, fPath))
        return false;
    
    return program->link();
}

bool MyGLWidget::setActiveShader (const QString& name)
{
    if (Globe::m_shaders.contains (name))
    {
        m_program = Globe::m_shaders[name];
        update(); // Trigger a repaint with the new pipeline
    }
    else
    {
        qDebug() << "Shader does not exist: " << name;
        return false;
    }
    
    return true;
}

bool MyGLWidget::registerShader (const QString& name, const QString& vFile, const QString& fFile)
{
    bool result = true;

    QOpenGLShaderProgram* prog = new QOpenGLShaderProgram (this);

    if (prog->addShaderFromSourceFile (QOpenGLShader::Vertex, vFile) &&
            prog->addShaderFromSourceFile (QOpenGLShader::Fragment, fFile) &&
            prog->link())
    {
        Globe::m_shaders.insert (name, prog);
        qDebug() << "Successfully registered shader:" << name;
    }
    else
    {
        qDebug() << "Failed to link shader" << name << ":" << prog->log();
        result = false;
    }

    return false;
}

void MyGLWidget::initCapitals (QString filename)
{
    std::cout << "Reading .csv file" << std::endl;

    MainWindow::instance()->logMessage (QString ("Opening City file: %1")
                                        .arg (filename));

    Globe::m_capitals.clear();

    // Verify file existence and readability before instantiating QFile
    QFileInfo checkFile (filename);

    if (!checkFile.exists() || !checkFile.isFile())
    {
        std::cout << "File does not exist" << std::endl;

        MainWindow::instance()->logMessage (QString ("CRITICAL: %1 not found at %2")
                                            .arg (filename)
                                            .arg (checkFile.absoluteFilePath()));
        return;
    }

    // Safe instantiation
    QFile file (filename); 

    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
    {
        std::cout << "Failed to open file" << std::endl;

        MainWindow::instance()->logMessage (QString ("ERROR: Could not open %1. Reason: %2")
                                            .arg (filename)
                                            .arg (file.errorString())
                                           );
        return;
    }

    std::cout << "File opened" << std::endl;
    MainWindow::instance()->logMessage ("Successfully opened " + filename);

    QTextStream in (&file);
    // Skip header line if your CSV has one
    if (!in.atEnd()) in.readLine(); 

    while (!in.atEnd())
    {
        QString line = in.readLine();
        QStringList fields = line.split (","); // Use ';' if your CSV uses semicolons
        
        if (fields.size() >= 6)
        {
            Globe::City city;
            // Adjust indices based on your CSV structure (Name, Lat, Lon, Population, etc.)
            city.name = fields[0].trimmed().remove ('"');
            city.lat = fields[3].toFloat();
            city.lon = fields[4].toFloat();
            city.extraInfo = "Population: " + fields[5].trimmed();

            Globe::m_capitals.push_back (city);
        }
    }

    file.close();

    MainWindow::instance()->logMessage (QString ("Loaded %1 cities.").arg (Globe::m_capitals.size()));
/*
    m_capitals =
    {
        {"Denver, CO", 39.7392, -104.9903},
        {"Augusta, ME", 44.3106, -69.7795},
        {"Sacramento, CA", 38.5816, -121.4944},
        {"Tallahassee, FL", 30.4383, -84.2807},
        {"Austin, TX", 30.2672, -97.7431},
        {"Albany, NY", 42.6526, -73.7562},
        {"NULL ISLAND", 0.0, 0.0}
        // ... add the rest here
    };
*/
}

void MyGLWidget::initSatellites()
{
    // Use Raw String Literals R"(...)" to ensure no escape-character issues
std::string l1 = R"(1 25544U 98067A   26116.51782528  .00002182  00000-0  10000-3 0  9993)";
std::string l2 = R"(2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537)";
    
    try
    {
        // 2. Create the Tle object
        libsgp4::Tle tle ("ISS", l1, l2);

        // 3. Instantiate the SGP4 propagator into your unique_ptr
        // This is where m_issPropagator finally stops being null
        m_issPropagator = std::make_unique<libsgp4::SGP4> (tle);

        MainWindow::instance()->logMessage ("ISS Propagator initialized successfully.");
    }
    catch (const std::exception& e)
    {
        MainWindow::instance()->logMessage (QString ("SGP4 Error: %1").arg (e.what()));
    }
}

void MyGLWidget::updateSatellitePhysics (qint64 msecs)
{
    if (!m_issPropagator)
    {
        MainWindow::instance()->logMessage ("ISS Propagator not initialized.");
        return;
    }

    // Use the modern Qt 6 static method
    QDateTime qtTime = QDateTime::fromMSecsSinceEpoch (msecs, Qt::UTC);
        
    // SGP4 DateTime uses (Year, Month, Day, Hour, Minute, Second)
    libsgp4::DateTime dt (qtTime.date().year(),
                          qtTime.date().month(),
                          qtTime.date().day(), 
                          qtTime.time().hour(),
                          qtTime.time().minute(),
                          qtTime.time().second()
                         );

// libsgp4::DateTime dt (2024, 4, 25, 12, 0, 0); 

    // 2. Propagate
    libsgp4::Eci eci = m_issPropagator->FindPosition (dt);
    
    // CRITICAL: Eci coordinates are Inertial (they don't rotate with Earth).
    // ToGeodetic() converts them to Lat/Lon/Alt FIXED to the rotating Earth.
    libsgp4::CoordGeodetic geo = eci.ToGeodetic();

    // LOG TO CONSOLE FOR DEBUG
/*    QString debugMsg = QString ("ISS - Lat: %1, Lon: %2, Alt: %3km | XYZ: %4, %5, %6")
                                .arg (qRadiansToDegrees (geo.latitude), 0, 'f', 2)
                                .arg (qRadiansToDegrees (geo.longitude), 0, 'f', 2)
                                .arg (geo.altitude, 0, 'f', 1)
                                .arg (m_issPos.x(), 0, 'f', 2)
                                .arg (m_issPos.y(), 0, 'f', 2)
                                .arg (m_issPos.z(), 0, 'f', 2);

    MainWindow::instance()->logMessage (debugMsg);
*/
    // If FindPosition fails, geo coordinates will be 0,0,0 (Center of Earth).
//    if (std::isnan (geo.latitude) || (geo.latitude == 0 && geo.longitude == 0))
//    {
//        // Log error to your new console
//        MainWindow::instance()->logMessage ("SGP4 Propagation Failed: Check TLE strings.");
//        return;
//    }

    float altMultiplier = (6371.0f + (float)geo.altitude) / 6371.0f;
    m_issPos = Utility::latLonToXYZ (Globe::m_liveOffset,
                                     qRadiansToDegrees (geo.latitude), 
                                     qRadiansToDegrees (geo.longitude), 
                                     Globe::globeRadius * altMultiplier
                                    );
}
