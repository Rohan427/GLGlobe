#include "MyGLWidget.hxx"
#include "MainWindow.hxx"


namespace SimCore
{
    void MyGLWidget::initializeGL() 
    {
        SIM_LOG (LM_INFO, "Initializing GL pipeline");

        // Initialize entity manager
        m_entityManager = new SimCore::EntityManager();
        m_entityManager->m_updatingEntities = false;

        int numThreads = std::thread::hardware_concurrency(); 
        if (numThreads == 0) numThreads = 16; // Fallback

        initializeOpenGLFunctions(); // Required in Qt to access gl* calls

        // Initialize satellite VBO
        m_satPositions.reserve (MAX_SATELLITES * sizeof (QVector3D));
        glGenVertexArrays (1, &m_satVao);
        glGenBuffers (1, &m_satVbo);

        glBindVertexArray (m_satVao);
        glBindBuffer (GL_ARRAY_BUFFER, m_satVbo);

        // Pre-allocate space for, say, 50,000 satellites
        glBufferData (GL_ARRAY_BUFFER, MAX_SATELLITES * sizeof (QVector3D), nullptr, GL_STREAM_DRAW);

        glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, sizeof (QVector3D), (void*)0);
        glEnableVertexAttribArray (0);

        glBindVertexArray (0);


        // Initialize City VBO
        glGenVertexArrays (1, &m_cityVao);
        glGenBuffers (1, &m_cityVbo);

        glBindVertexArray (m_cityVao);

        initCapitals ("/home/pgallen/Downloads/capitals.csv");

        // Generate and Bind VBO
        glBindBuffer (GL_ARRAY_BUFFER, m_cityVbo);
        glBufferData (GL_ARRAY_BUFFER, Globe::m_capitals.size() * sizeof (Globe::City), Globe::m_capitals.data(), GL_STATIC_DRAW);

        // Now OpenGL "saves" this configuration into the active m_cityVao
        glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, sizeof (Globe::City), (void*)0);
        glEnableVertexAttribArray (0);

        // Unbind to prevent accidental state changes later
        glBindVertexArray (0);



        // These two lines enable 3D depth testing
        glEnable (GL_DEPTH_TEST);
        glDepthFunc (GL_LESS);
        glFrontFace (GL_CW);
        
        // Enable MSAA
        glEnable (GL_MULTISAMPLE);

        // If black screen appears after moving to 4.3, add this to initializeGL
        m_vao.create();
        m_vao.bind();

        // 1. Simple Shaders (Passes texture and coordinates)
        m_program = new QOpenGLShaderProgram (this);

    //    registerShader ("Standard", "shaders/Earth.vert", "shaders/Earth.frag");
        registerShader ("NightLights", "shaders/Earth.vert", "shaders/Earth-night.frag");
        registerShader ("BumpLights", "shaders/Earth-Bump.vert", "shaders/Earth-Bump.frag");
        registerShader ("Satellites", "shaders/Satellite.vert", "shaders/Satellite.frag");
        registerShader ("CityPoints", "shaders/Simple-Point.vert", "shaders/Simple-Point.frag");
        registerShader ("CityFonts", "shaders/CityLabel.vert", "shaders/CityLabel.frag");

        // Set the default
        m_program = Globe::m_shaders["BumpLights"];

        if (!m_program->link())
        {
            SIM_LOG (LM_ERROR, QString ("Shader Linker Error: %1").arg (m_program->log()));
        }

        generateSphere (Globe::globeRadius, Globe::globeSectors, Globe::globeStacks);

        m_vbo.create();
        m_vbo.bind();

        // 3D sphere
        m_vbo.allocate (m_sphereVertices.data(), m_sphereVertices.size() * sizeof (float));

        SIM_LOG (LM_INFO, "Load texture");
        //textureID = loadTexture (mapSizes.huge, "textures/1_earth_16k.jpg");
        
        if (!loadTextureFiles (Globe::mapSizes.huge))
        {
            SIM_LOG (LM_CRITICAL, "FATAL ERROR: Failed to initialize textures");
            QCoreApplication::exit (1); // Exit app
            return;
        }

        dayTextureID =   Globe::textureMap["earthncice16k"];
        nightTextureID = Globe::textureMap["earthnight16k"];
        bumpTextureID =  Globe::textureMap["earthbump16k"];
        fontTexture = Globe::textureMap["arialFont"];
        //textureID = textureMap["earth16k"];


        // Fonts
        m_fontManager = new Globe::FontManager();
        m_fontManager->loadArialFont ("fonts/arial.fnt");

        glGenVertexArrays (1, &m_fontManager->m_labelVao);
        glBindVertexArray (m_fontManager->m_labelVao);

        glGenBuffers (1, &m_fontManager->m_labelVbo);

        m_fontManager->buildLabelVBO (Globe::m_capitals); 

        // Bind and Upload
        glBindBuffer (GL_ARRAY_BUFFER, m_fontManager->m_labelVbo);

        int stride = sizeof(Globe::LabelVertex); // This will now be exactly 28 bytes

        glBufferData (GL_ARRAY_BUFFER, Globe::m_cityLabels.size() * stride, Globe::m_cityLabels.data(), GL_STATIC_DRAW);

        // onfigure Layout (Must match your struct: 3 floats for anchor, 2 for UV, 2 for Offset)
        // Location 0: anchor (x,y,z)
        glEnableVertexAttribArray (0);
        glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

        // Location uv (u,v)
        glEnableVertexAttribArray (1);
        glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof (float)));

        // Location offset (ox,oy)
        glEnableVertexAttribArray (2);
        glVertexAttribPointer (2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof (float)));

        glBindVertexArray (0); // Clean up state

//        std::cout << "Texture ID is " << textureID << std::endl;

        initializeGlobePosition();

        Globe::timer.start();
        
        //Access the satellite source from MainWindow
        m_satelliteSource = MainWindow::instance()->getSatelliteSource();

        // 1. Get the action from MainWindow
        QAction* updateSatsAct = MainWindow::instance()->getUpdateSatsAct();
        connect (updateSatsAct, &QAction::triggered, [this]()
                    {
                        // Access the network source and trigger the update
                        auto* m_satelliteSource = MainWindow::instance()->getSatelliteSource();
                        if (m_satelliteSource) m_satelliteSource->requestGroup();
                    }
                );

        //Connect NOW that we know m_entityManager is not null
        bool success = connect (m_satelliteSource, &Network::BaseDataSource::dataReceived,
                                m_entityManager, &SimCore::EntityManager::processTleData,
                                Qt::UniqueConnection); // <--- This prevents the signal from firing twice
        
        if (success)
        {
            SIM_LOG (LM_INFO, "Network-to-Simulation bridge connected.");
        }

        // Activate the 32 ACE threads
        m_entityManager->startSimulation (Globe::MAX_THREADS);
    }


    void MyGLWidget::paintGL() 
    {
//        std::cout << "paintGL entered" << std::endl;
        // Compute shader code
    //    m_computeProgram->bind();
    //    m_computeProgram->setUniformValue ("time", (float)timer.elapsed() / 1000.0f);
        
        // Bind texture to Image Unit 0 (matching 'binding = 0' in shader)
    //    glBindImageTexture (0, textureID, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        
        // Dispatch enough threads to cover a 512x512 texture (512/16 = 32 groups)
    //    glDispatchCompute (512 / 16, 512 / 16, 1);
        
        // Ensure compute finishes before the fragment shader tries to read it
    //    glMemoryBarrier (GL_SHADER_IMAGE_ACCESS_BARRIER_BIsetActiveShaderT);
    //    m_computeProgram->release();

        // End compute shader code


        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable (GL_DEPTH_TEST);
        glDepthFunc (GL_LESS);
        glEnable (GL_CULL_FACE);

        setActiveShader ("BumpLights");

        // Projection (The 4K Lens)
        float aspect = (float)width() / (float)height();
        float currentFov = Globe::g_perspective * Globe::m_zoom; 
        QMatrix4x4 projection;
        projection.perspective (Globe::DEFAULT_PERSPECTIVE, aspect, 0.1f, 100.0f);

        // View (The Camera/Mouse controls)
        QMatrix4x4 view;
        view.translate (Globe::m_offset.x(), Globe::m_offset.y(), -10.0f * Globe::m_zoom);

        // These rotations let the mouse "orbit" the globe
        view.rotate (Globe::m_rotation.x(), 1.0f, 0.0f, 0.0f);
        view.rotate (Globe::m_rotation.y(), 0.0f, 1.0f, 0.0f);

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
        glBindTexture (GL_TEXTURE_2D, bumpTextureID);
        m_program->setUniformValue ("bumpSampler", 2);

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
        glBindVertexArray (0);

        m_vao.release();
        m_program->release();

        /******************** Draw satellites *******************/
        // 1. Gather latest positions from ACE threads
        if (setActiveShader ("Satellites"))
        {
            if (SimCore::EntityManager::m_vectorLock.tryacquire() == 0)
            {
                m_satPositions.clear(); 
                const auto& entities = m_entityManager->getEntities();

                for (auto* entity : entities)
                {
                    if (entity) m_satPositions.push_back(entity->getPosition());
                }

                SimCore::EntityManager::m_vectorLock.release();
                
                // 2. Only upload to GPU if we actually refreshed the data
                glBindBuffer (GL_ARRAY_BUFFER, m_satVbo);
                // Orphan and upload
                glBufferData (GL_ARRAY_BUFFER, MAX_SATELLITES * sizeof(QVector3D), nullptr, GL_STREAM_DRAW);
                glBufferSubData (GL_ARRAY_BUFFER, 0, m_satPositions.size() * sizeof(QVector3D), m_satPositions.data());
            }

            // Draw all satellites in ONE call
            if (!m_satPositions.empty())
            {
                m_program->bind();
                m_program->setUniformValue ("mvp", mvp); //projection * view * model);
                m_program->setUniformValue ("satColor", QVector3D (1.0f, 0.0f, 1.0f)); // Magenta

                glEnable (GL_PROGRAM_POINT_SIZE); // Enables gl_PointSize from shader
                glEnable (GL_BLEND);
                glBlendFunc (GL_SRC_ALPHA, GL_ONE); // Additive blend makes them "glow"
                
                glBindVertexArray (m_satVao);
                glDepthFunc (GL_LEQUAL); 

    //            std::cout << "Drawing " << m_satPositions.size() << " sats" << std::endl;

                glDrawArrays (GL_POINTS, 0, (GLsizei)m_satPositions.size());

                glBindVertexArray (0);
                glDisable (GL_BLEND);

                m_program->release();

//                std::cout << "     rendering done" << std::endl;
            }
        }
        /*********************** END SATILLITES *********************/


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

        ///////////////////// City labels //////////////////////////
        if (Globe::m_showCities)
        {
            if (setActiveShader ("CityPoints"))
            {
                m_program->bind();
                m_program->setUniformValue ("mvp", mvp);
                
                glEnable (GL_PROGRAM_POINT_SIZE); // Enables gl_PointSize from shader
                glEnable (GL_BLEND);
                glBlendFunc (GL_SRC_ALPHA, GL_ONE);
                glBindVertexArray (m_cityVao);

                glDepthFunc (GL_LEQUAL); 

                // Draw all cities in a single ultra-fast call
                glDrawArrays (GL_POINTS, 0, Globe::m_cityCount);
                glBindVertexArray (0);
                glDisable (GL_BLEND);

                m_program->release();
            }

            if (setActiveShader ("CityFonts"))
            {
                m_program->bind();
                m_program->setUniformValue ("viewportSize", QVector2D (width(), height()));
                m_program->setUniformValue ("scale", 0.45f); 
                m_program->setUniformValue ("mvp", mvp);

                // Bind Day Texture to Unit 0
                glActiveTexture (GL_TEXTURE0);
                glBindTexture (GL_TEXTURE_2D, fontTexture);
                m_program->setUniformValue ("arialFont", 0);

                // Scale should be based on your viewport size and zoom level
                // Example: 1.0 / windowHeight * zoomFactor
                ////float m_currentLabelScale = 1.0f / (float)height() * Globe::m_zoom;
                ////m_program->setUniformValue ("scale", m_currentLabelScale);

                glEnable (GL_BLEND);
                glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthFunc (GL_LEQUAL); 

                glBindVertexArray (m_fontManager->m_labelVao);

                glDrawArrays (GL_TRIANGLES, 0, m_fontManager->m_labelVertexCount);

                glBindVertexArray (0);
            }

            /*// Paint test (a large point on the North Pole, always visible ///
        
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
            ***************** END TEST **********************/

            glDisable (GL_DEPTH_TEST);
            glDisable (GL_CULL_FACE);
            QPainter painter (this);

            QFont font ("Arial", Globe::m_fontSize, QFont::Normal); // Specifically name a common font
            font.setHintingPreference (QFont::PreferNoHinting); // Prevents expensive glyph caching
            painter.setFont (font);

            painter.beginNativePainting();

            painter.setRenderHint (QPainter::Antialiasing);
            QRect viewport (0, 0, width(), height());

            for (const auto& city : Globe::m_capitals)
            {
                QVector3D worldPos = Utility::latLonToXYZ (Globe::m_liveOffset, city.lat, city.lon, Globe::cityLabelHeight);
                QVector4D clipPos = mvp * QVector4D (worldPos, 1.0f);

                // Set up the Pen (for the outline and text)
                QPen myPen (Globe::m_textColor);
                myPen.setWidth (1); 

                // Set up the shadow pen
                QPen shadowPen (Globe::m_shadowColor);
                shadowPen.setWidth (7);

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
//                                painter.setFont (QFont ("Arial", Globe::m_fontSize, QFont::Bold));
                                painter.drawText (cardRect.adjusted (10, 10, -10, -10), Qt::AlignTop, Globe::m_selectedCity->name);
                                
//                                painter.setFont (QFont ("Arial", Globe::m_fontSize));
                                painter.drawText (cardRect.adjusted (10, 35, -10, -10), Qt::AlignTop, Globe::m_selectedCity->extraInfo);
                            }
                        }
                    } // if (ndcZ >= -1.0f && ndcZ <= 1.0f)
                } // if (clipPos.w() != 0.0f)
            } // for (const auto& city : m_capitals)
            
            painter.endNativePainting(); 
            painter.end();

        } // if (m_showCities)


        /******************* end painter ***************/

        updateStatus();
    } // END: MyGLWidget::paintGL() 

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
        Globe::m_zoom *= delta;

        if (Globe::m_zoom < ZOOM_CLAMP)
        {
            Globe::m_zoom = ZOOM_CLAMP;
        }

        updateStatus();
    }

    void MyGLWidget::mouseMoveEvent (QMouseEvent *event)
    {
        float adaptiveSens = sensitivity * Globe::m_zoom; 
        float currentAspect = (float)width() / (float)height();

        if (event->buttons() & Qt::LeftButton)
        {
            QPoint diff = event->pos() - m_lastMousePos;
            // Dragging logic (adjust sensitivity as needed)
            Globe::m_offset += QVector2D (
                                    (diff.x() * currentAspect * adaptiveSens) / (float)width(),
                                    (-diff.y() * adaptiveSens) / (float)height()
                                  );
        }
        else if (event->buttons() & Qt::RightButton)
        {
            QPoint diff = event->pos() - m_lastMousePos;
            // Rotation logic
            adaptiveSens = rotSensitivity * Globe::m_zoom;
            Globe::m_rotation += QVector2D (diff.y() * adaptiveSens, diff.x() * adaptiveSens);
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
        Globe::m_zoom = Globe::DEFAULT_ZOOM;
        Globe::m_offset = Globe::DEFAULT_OFFSET;
        Globe::m_liveOffset = Globe::DEFAULT_LIVEOFFSET;
        Globe::m_liveTilt = Globe::DEFAULT_TILT;
        Globe::m_rotation = Globe::DEFAULT_ROTATION;
        Globe::m_ambientLevel = Globe::DEFAULT_AMBIENT;

        SIM_LOG (LM_INFO, "Globe reset to default position");
    }

    GLuint MyGLWidget::loadTexture (std::array<int, 2>& mapSize, const QString& filePath, const int type = 1)
    {
        if (type < 1)
        {
            SIM_LOG (LM_CRITICAL, "Invalid texture type");
            return 0;
        }

        if (filePath == nullptr)
        {
            SIM_LOG (LM_CRITICAL, "Empty or null texture file path");
        }

        QImageReader reader (filePath);
        
        // Bypass the default 128MB limit for an 8k texture
        reader.setAllocationLimit (1024); 

        if (!reader.canRead())
        {
            SIM_LOG (LM_CRITICAL, QString ("Cannot read image: %1").arg (reader.errorString()));
            return 0;
        }

        if (type == 1)
        {
            // Optional: Downscale during load to stay within ROCm memory stability limits
            //if (reader.size().width() > 8192)
            {
                reader.setScaledSize (QSize (mapSize[0], mapSize[1]));
            }
        }

        QImage img = reader.read();

        if (img.isNull())
        {
            SIM_LOG (LM_CRITICAL, QString ("Load failed: %1").arg (reader.errorString().toStdString().c_str()));
            return 0;
        }
        else
        {
            SIM_LOG (LM_INFO, QString ("Reading texture file %1").arg (filePath));
        }

        if (type == 1)
        {
            // Convert to RGBA8888 for GL_RGBA8 compatibility
            // Use flipped() to move the origin from top-left to bottom-left for OpenGL
            img = img.convertToFormat (QImage::Format_RGBA8888).flipped (Qt::Horizontal);
        }
        else if (type == 2)
        {
            img = img.convertToFormat (QImage::Format_RGBA8888);
        }

        GLuint textureID;
        glGenTextures (1, &textureID);
        glBindTexture (GL_TEXTURE_2D, textureID);

        if (type == 1)
        {
            // Texture parameters for the globe
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        else if (type == 2)
        {
            // SDF Font Specifics: LINEAR filtering is mandatory for smooth scaling.
            // We disable Mipmaps for SDF fonts to keep the distance field edges sharp.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        // Upload to the RX 7800XT
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, 
                     img.width(), img.height(), 0, 
                     GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());

        glGenerateMipmap (GL_TEXTURE_2D);

        return textureID;
    }

    bool MyGLWidget::loadTextureFiles (std::array<int, 2>& mapSize)
    {
        for (int i = 0; i < Globe::TextureList.size(); i++)
        {
            std::map<std::string, QString> texture = Globe::TextureList.at (i);

            if (texture.empty())
            {
                SIM_LOG (LM_CRITICAL, "No map found");
                return false;
            }

            for (const auto& pair : Globe::TextureList.at (i))
            {
                // Indexes start at 0, but types start at 1
                GLuint textureID = loadTexture (mapSize, pair.second, i+1);
                
                if (textureID > 0)
                {
                    Globe::textureMap.insert ({pair.first, textureID});
                }
                else
                {
                    SIM_LOG (LM_CRITICAL, "Fatal error: Texure ID is 0"); 
                    return false;
                }
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
//            update(); // Trigger a repaint with the new pipeline
        }
        else
        {
            SIM_LOG (LM_CRITICAL, QString ("ERROR: Shader does not exist: %1").arg (name.toStdString()));
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
//            SIM_LOG (LM_INFO, QString ("Successfully registered shader: %1").arg (name.toStdString()));
        }
        else
        {
            SIM_LOG (LM_CRITICAL, QString ("CRITICAL: Failed to link shader: %1, %2")
                                          .arg (name.toStdString())
                                          .arg (prog->log().toStdString())
                    );
            result = false;
        }

        return result;
    }

    void MyGLWidget::initCapitals (QString filename)
    {
        SIM_LOG (LM_INFO, QString ("Opening City file: %1").arg (filename));

        Globe::m_capitals.clear();

        // Verify file existence and readability before instantiating QFile
        QFileInfo checkFile (filename);

        if (!checkFile.exists() || !checkFile.isFile())
        {
            SIM_LOG (LM_ERROR, QString ("CRITICAL: %1 not found at %2")
                                       .arg (filename)
                                       .arg (checkFile.absoluteFilePath())
                    );
            return;
        }

        // Safe instantiation
        QFile file (filename); 

        if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
        {
            SIM_LOG (LM_ERROR, QString ("ERROR: Could not open %1. Reason: %2")
                             .arg (filename)
                             .arg (file.errorString())
                             );
            return;
        }

        SIM_LOG (LM_INFO, "Successfully opened " + filename);

        QTextStream in (&file);
        // Skip header line if your CSV has one
        if (!in.atEnd()) in.readLine(); 

        SIM_LOG (LM_INFO, "Parsing cities from " + filename);

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

                city.position = Utility::latLonToXYZ (Globe::m_liveOffset, city.lat, city.lon, Globe::cityLabelHeight);

                Globe::m_capitals.push_back (city);
            }

            Globe::m_cityCount = Globe::m_capitals.size();
        }

        file.close();

        SIM_LOG (LM_INFO, QString ("Loaded %1 cities.").arg (Globe::m_capitals.size()));
    }

    void MyGLWidget::checkLocalCache (const QString& groupKey)
    {
        std::cout << "Checking data cache " << groupKey.toStdString() << std::endl;

        QDir dir (Globe::DATA_DIR_PATH);
        QString filter = QString ("satellites_%1_*.tle").arg (groupKey.toLower());

        QStringList filters;
        filters << "satellites_*.tle";
        
        // Sort by Time to ensure files.first() is the newest
        QFileInfoList files = dir.entryInfoList ({filter}, QDir::Files, QDir::Time);

        if (!files.isEmpty())
        {
            std::cout << "Files found..." << std::endl;

            QFileInfo latest = files.first();

            // Check for old files and delete them
            for (const QFileInfo& info : files)
            {
                if (info.lastModified().daysTo (QDateTime::currentDateTime()) > 2)
                {
                    QFile::remove (info.absoluteFilePath());
                    SIM_LOG (LM_INFO, "Cleaned up old cache file: " + info.fileName());
                }
            }

//            std::cout << "Loading file..." << std::endl;

            latest = files.first();
            qint64 secsOld = latest.lastModified().toUTC().secsTo (QDateTime::currentDateTimeUtc());

            if (secsOld < 7200)
            { // 2 Hours = 7200 seconds
//                std::cout << "Using fresh local cache: " << latest.fileName().toStdString() << std::endl;

                m_entityManager->processTleData ("FILE_READY:" + latest.absoluteFilePath(), groupKey);

                return;
            }
        }
        
        SIM_LOG (LM_INFO, "Requesting new data...");

        // If no files or they are old, trigger a fresh download
        m_satelliteSource->requestGroup (groupKey);
    }
}    
