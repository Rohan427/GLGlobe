#include "MyGLWidget.hxx"
#include "MainWindow.hxx"
#include "Tracking.hxx"
#include "DataObjects.hxx"


namespace SimCore
{
    void MyGLWidget::initializeGL() 
    {
        SIM_LOG (LM_INFO, "Initializing GL pipeline");

        // 1. EXTRACT THE ACTIVE RENDERING CONTEXT POINTER
        QOpenGLContext* currentCtx = QOpenGLContext::currentContext();

        if (!currentCtx)
        {
            SIM_LOG (LM_CRITICAL, "CRITICAL: No active QOpenGLContext found during initialization!");
            return;
        }

        // 2. INITIALIZE GLAD 2 VIA TYPE-SAFE ADAPTER LAMBDA
        // The lambda provides a standalone C-style signature callback function hook that 
        // captures the current context pointer and executes the member function safely.
        auto gladProcLoader = [](const char* name) -> void*
        {
            QOpenGLContext* ctx = QOpenGLContext::currentContext();

            if (ctx)
            {
                // Convert C-string parameter into Qt's format and query the graphics driver address
                return reinterpret_cast<void*> (ctx->getProcAddress (name));
            }

            return nullptr;
        };

        // 3. LOAD OPENGL 4.6 CORE HOOKS USING THE CORRECT GLAD 2 NAMING SCHEMA
        // Pass the lambda wrapper function pointer straight into the glad loader pipeline
        if (!gladLoadGL (reinterpret_cast<GLADloadfunc> (+gladProcLoader)))
        {
            SIM_LOG (LM_CRITICAL, "CRITICAL: GLAD 2 failed to resolve OpenGL 4.6 Core function pointers!");
            QCoreApplication::exit (-1);

            std::exit (-1); 
        }

        SIM_LOG (LM_INFO, "GLAD 2 initialized successfully. OpenGL 4.6 Core Driver hooks active.");

        // Initialize important variables from defaults
        Globe::g_perspective = ::Config::getInstance().DEFAULT_PERSPECTIVE;
        Globe::m_liveOffset = ::Config::getInstance().DEFAULT_LIVEOFFSET;
        Globe::m_liveTilt = ::Config::getInstance().DEFAULT_TILT;
        Globe::m_rotation = ::Config::getInstance().DEFAULT_ROTATION; // x = pitch, y = yaw
        Globe::m_zoom = ::Config::getInstance().DEFAULT_ZOOM;
        Globe::m_offset = ::Config::getInstance().DEFAULT_OFFSET;   // for dragging
        Globe::m_ambientLevel = ::Config::getInstance().DEFAULT_AMBIENT;

        // Default radius and stacks (set to 1.0, 64, 64 in the default config file)
        Globe::globeRadius = ::Config::getInstance().DEFAULT_RADIUS;
        Globe::globeSectors = ::Config::getInstance().DEFAULT_SECTORS;
        Globe::globeStacks = ::Config::getInstance().DEFAULT_STACKS;

        // Setup constant Earth radius using in SGP4 internal value
        Globe::earthRadiusKm = libsgp4::kXKMPER;
        Globe::glScaleFactor = Globe::globeRadius / static_cast<float> (Globe::earthRadiusKm);

        // Height of labels and points above the globe. Put labels above globe, but not too far or they will "slide" due
        // to perspective and zoom changes
        Globe::cityLabelHeight = Globe::globeRadius + ::Config::getInstance().LABEL_HEIGHT_OFFSET;

        Globe::m_markerSize = ::Config::getInstance().DEFAULT_MARKER_SIZE;
        Globe::m_fontSize = ::Config::getInstance().DEFAULT_FONT_SIZE; // Default size

        std::cout << "Initialize Entity Manager" << std::endl;

        // Initialize entity manager
        m_entityManager = new SimCore::EntityManager();
        m_entityManager->m_updatingEntities = false;

        // Initialize entity SSBO
        if (!allocateSimulationSSBO (::Config::getInstance().MAX_OBJECTS))
        {
            SIM_LOG (LM_CRITICAL, "FATAL INTERLOCK: Persistent SSBO allocations failed. Exiting Application.");
            
            // Force a clean runtime exit to protect system drivers from memory corruption
            QCoreApplication::exit (-1);

            std::exit (-1); 
        }

        m_entityManager->initializeSatelliteBufferSlots();

        ::glGenVertexArrays (1, &m_dummyVaoId);
        ::glBindVertexArray (m_dummyVaoId);
        // We leave it empty—no attributes enabled, no offsets set. 
        ::glBindVertexArray (0);

        SIM_LOG (LM_INFO, "Core Profile Validation: Clean dummy VAO initialized successfully.");

        int numThreads = std::thread::hardware_concurrency(); 
        if (numThreads == 0) numThreads = 16; // Fallback

        initializeOpenGLFunctions(); // Required in Qt to access gl* calls

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

        // If black screen appears after moving to 4.6, add this to initializeGL
        m_vao.create();
        m_vao.bind();

        // 1. Simple Shaders (Passes texture and coordinates)
        m_program = new QOpenGLShaderProgram (this);

        // Globe shaders
        registerShader ("NightLights", "shaders/Earth.vert.spv", "shaders/Earth-night.frag.spv");
        registerShader ("BumpLights", "shaders/Earth-Bump.vert.spv", "shaders/Earth-Bump.frag.spv");
        registerShader ("CityPoints", "shaders/Simple-Point.vert.spv", "shaders/Simple-Point.frag.spv");
        registerShader ("RangeRings", "shaders/Sensor-Sphere.vert.spv", "shaders/Sensor-Sphere.frag.spv");

        // Object shaders
        registerShader ("Satellites", "shaders/Satellite.vert.spv", "shaders/Satellite.frag.spv");
        registerShader ("MissilePaths", "shaders/MissilePath.vert.spv", "shaders/MissilePath.frag.spv");

        // Font shaders
        registerShader ("CityFonts", "shaders/CityLabel.vert.spv", "shaders/CityLabel.frag.spv");

        // Physics compute shader
        registerShader ("PhysicsEngine", "shaders/physics_engine.comp.spv", ""); // Compute has empty fragment parameter
        

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
        
        if (!Utility::loadTextureFiles (Globe::mapSizes.huge))
        {
            SIM_LOG (LM_CRITICAL, "FATAL ERROR: Failed to initialize textures");
            QCoreApplication::exit (1); // Exit app
            return;
        }

        dayTextureID =   Globe::textureMap["earthncice16k"];
        nightTextureID = Globe::textureMap["earthnight16k"];
        bumpTextureID =  Globe::textureMap["earthbump16k"];
        fontTexture = Globe::textureMap["arialFont"];


        // Fonts
        m_fontManager = new Globe::FontManager();
        m_fontManager->loadArialFont ("fonts/arial.fnt");

        glGenVertexArrays (1, &m_fontManager->m_labelVao);
        glBindVertexArray (m_fontManager->m_labelVao);

        glGenBuffers (1, &m_fontManager->m_labelVbo);

        m_fontManager->buildLabelVBO (Globe::m_capitals); 

        // Bind and Upload
        glBindBuffer (GL_ARRAY_BUFFER, m_fontManager->m_labelVbo);

        int stride = sizeof (Globe::LabelVertex); // This will now be exactly 28 bytes

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
                                Qt::UniqueConnection // <--- This prevents the signal from firing twice
                               );
        
        if (success)
        {
            SIM_LOG (LM_INFO, "Network-to-Simulation bridge connected.");
        }

        // Activate the simulation threads
        m_entityManager->startSimulation (::Config::getInstance().MAX_FPU_THREADS);

        this->setFocusPolicy (Qt::StrongFocus);
    }





    void MyGLWidget::paintGL() 
    {
        SIM_LOG (LM_DEBUG, "paintGL");

        // CRITICAL TIMING CLOCK: INITIALIZE THE PERSISTENT MASTER CLOCK CONTAINER ON THE FIRST FRAME
        if (!m_frameTimer.isValid())
        {
            m_frameTimer.start();
        }

        // 2. EXTRACT RAW ELAPSED NANOSECONDS SINCE THE LAST FRAME DREW
        qint64 elapsedNano = m_frameTimer.nsecsElapsed();
        m_frameTimer.restart(); // Reset the clock instantly for the next pass

        // 3. CONVERT TO FRACTIONAL FLOATING-POINT SECONDS
        // At 3,000 FPS, this evaluates to exactly ~0.0003333f seconds.
        // At a stable 60 FPS, this evaluates to exactly ~0.0166667f seconds.
        this->m_masterDeltaTimeSec = static_cast<float> (elapsedNano) / 1000000000.0f;

        // 4. THE APPLICATION UNPAUSE FIREWALL
        // If you drag the window or the OS pauses execution, the delta will spike.
        // We clamp the maximum time step to 100ms to prevent objects from teleporting.
        if (m_masterDeltaTimeSec > 0.1f)
        {
            m_masterDeltaTimeSec = 0.001f; // Standard fallback safe interval step
        }

        /*************** Setup and draw the globe *****************/
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable (GL_DEPTH_TEST);
        glDepthFunc (GL_LESS);
        glEnable (GL_CULL_FACE);

        setActiveShader ("BumpLights");

        // Projection (The 4K Lens)
        float aspect = (float)width() / (float)height();
        float currentFov = Globe::g_perspective * Globe::m_zoom; 
        QMatrix4x4 projection;
        projection.perspective (::Config::getInstance().DEFAULT_PERSPECTIVE, aspect, 0.1f, 100.0f);

        // Compute the dynamic pull-back multiplier relative to the active radius configuration
        // This maintains the exact same proportional visual distance whether radius is 0.5, 1.0, or 1.5.
        float dynamicRadiusScalar = ::Config::getInstance().DEFAULT_RADIUS * 6.66667f;

        QMatrix4x4 view;

        // 2. Translate matrix using the safe, relative scalar
        view.translate (Globe::m_offset.x(), 
                        Globe::m_offset.y(), 
                        -dynamicRadiusScalar * Globe::m_zoom
                       );

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
       
        // Update Uniforms
        m_program->bind();
        m_program->setUniformValue (17, Globe::m_ambientLevel);
        m_program->setUniformValue (4, model);
        m_program->setUniformValue (16, QVector3D (0, 0, 1));
        m_program->setUniformValue (0, mvp);

        // Bind Day Texture to Unit 0
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, dayTextureID);
        m_program->setUniformValue (13, 0);

        // Bind Night Texture to Unit 1
        glActiveTexture (GL_TEXTURE1);
        glBindTexture (GL_TEXTURE_2D, nightTextureID);
        m_program->setUniformValue (14, 1);

        // Bind the Bump/Height Map
        glActiveTexture (GL_TEXTURE2);
        glBindTexture (GL_TEXTURE_2D, bumpTextureID);
        m_program->setUniformValue (15, 2);

        // Drawing
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

        m_program->release();

        /*************** Draw sensor rings (if enabled) *****************/

        SIM_LOG (LM_DEBUG, "paintGL Initialize sensor range");
        // Used for sensor filter
        float glDetectionRange = m_entityManager->m_tracker->m_detectionRange - Globe::globeRadius;

        // PULL THE METRIC POSITION DIRECTLY FROM THE CITY MARKER UNIFORM
        QVector3D localFilterCenter = m_entityManager->m_tracker->m_filterAnchor;

        // Calculate the true world position for the shader tracking uniform
        QVector4D rotatedCenter4 = model * QVector4D (localFilterCenter, 1.0f);
        QVector3D worldFilterCenter = rotatedCenter4.toVector3D();

        SIM_LOG (LM_DEBUG, "paintGL test if sensors are enabled");

        // For range rings
        if (m_entityManager->m_tracker->m_filterActive)
        {
            setActiveShader ("RangeRings");

            // 2. Setup Translucent Alpha Blending States
            glEnable (GL_BLEND);
            glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // CRITICAL: Disable depth writing to prevent concentric nested 3D hulls 
            // from clipping out or occluding smaller spheres inside them.
            glDepthMask (GL_FALSE);

            QMatrix4x4 invView = view.inverted();
            QVector3D extractedCameraPos = QVector3D (invView (0, 3), invView (1, 3), invView (2, 3));

            QVector3D ringColorVec = QVector3D(::Config::getInstance().RANGE_RING_COLOR.x(),
                                               ::Config::getInstance().RANGE_RING_COLOR.y(),
                                               ::Config::getInstance().RANGE_RING_COLOR.z()
                                              );

            // Convert your range spacing and max limits to matching fractional GL scales
            float glRingDelta      = m_entityManager->m_tracker->m_RngRingDelta;

            m_program->bind();
            m_program->setUniformValue (4, view); // view
            m_program->setUniformValue (8, projection); //projection
            m_program->setUniformValue (13, extractedCameraPos); // Vector3D tracking your camera pos cameraWorldPos
            m_program->setUniformValue (14, worldFilterCenter); // filterCenter
            m_program->setUniformValue (15, ringColorVec); // rangeRingColor
            m_program->setUniformValue (12, Globe::globeRadius); // globeRadius

            m_vao.bind();
            m_vbo.bind();
            int strideBytes = 8 * sizeof (float);

            // Map Location 0 -> Position Vector [X, Y, Z]
            m_program->enableAttributeArray (0);
            m_program->setAttributeBuffer (0, GL_FLOAT, 0, 3, strideBytes);

            float currentRadius = glRingDelta;
            int vertexCount = m_sphereVertices.size() / 8;

            QMatrix4x4 localRingModel;

            while (currentRadius < glDetectionRange)
            {
                // MATCH THE EARTH MESH TRANSFORMS EXACTLY
                // Rings must rotate with axial tilt and spin because filterCenter is a fixed feature point
                localRingModel = model;

                // Translate in local model space BEFORE applying rotations, then scale
                localRingModel.translate (localFilterCenter); 
                localRingModel.scale (currentRadius); 

                m_program->setUniformValue (0, localRingModel); // model

                glDrawArrays (GL_TRIANGLES, 0, vertexCount);

                currentRadius += glRingDelta;
            }

            // Always draw last ring
            localRingModel = model;

            // Translate in local model space BEFORE applying rotations, then scale
            localRingModel.translate (localFilterCenter); 
            localRingModel.scale (glDetectionRange); 

            m_program->setUniformValue (0, localRingModel); // model

            glDrawArrays (GL_TRIANGLES, 0, vertexCount);


            // Restore standard pipeline rendering state configurations
            glDisableVertexAttribArray (0);
            glDisableVertexAttribArray (1);

            m_vao.release();
            m_vbo.release();

            glDepthMask (GL_TRUE);
            glDisable (GL_BLEND);
        }

        /*************** Draw satellites *****************/
        SIM_LOG (LM_DEBUG, "paintGL Begin satellite processing");

        renderSatellitePoints (mvp);

        // =============================================================================
        // NEW TEST CORE MILESTONE: DRAW ACTIVE BALLISTIC TRAJECTORIES
        // =============================================================================
        // This safely invokes your new pre-compiled MissilePaths SPIR-V shader pair
        renderMissileArcs (mvp);

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
                m_program->setUniformValue (0, mvp); // mvp
                
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
                m_program->setUniformValue (4, QVector2D (width(), height())); // viewportSize
                m_program->setUniformValue (5, 0.45f); // scale
                m_program->setUniformValue (0, mvp); //mvp

                // Bind Day Texture to Unit 0
                glActiveTexture (GL_TEXTURE0);
                glBindTexture (GL_TEXTURE_2D, fontTexture);
                m_program->setUniformValue ("arialFont", 0);

                glEnable (GL_BLEND);
                glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthFunc (GL_LEQUAL); 

                glBindVertexArray (m_fontManager->m_labelVao);

                glDrawArrays (GL_TRIANGLES, 0, m_fontManager->m_labelVertexCount);

                glBindVertexArray (0);
            }

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
                                painter.drawText (cardRect.adjusted (10, 10, -10, -10), Qt::AlignTop, Globe::m_selectedCity->name);
                                
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

        SIM_LOG (LM_DEBUG, "paintGL End");
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
        if (event->key() == Qt::Key_F)
        {
            if (Globe::m_selectedCity)
            {
                // Case 1 & 2: If a city is highlighted, move/place the filter there
                ACE_GUARD (ACE_Thread_Mutex, mon, m_entityManager->m_vectorLock);

                if (m_entityManager->m_tracker)
                {
                    m_entityManager->m_tracker->m_filterAnchor = Globe::m_selectedCity->position;
                    m_entityManager->m_tracker->m_filterActive = true;
                    SIM_LOG (LM_INFO, "Sensor filter placed at: " + Globe::m_selectedCity->name);
                    SIM_LOG (LM_INFO, QString ("Location:\n x %1,\n y %2,\n z %3\n")
                             .arg (Globe::m_selectedCity->position.x())
                             .arg (Globe::m_selectedCity->position.y())
                             .arg (Globe::m_selectedCity->position.z())
                            );
                }
            }
            else
            {
                // Case 3: If no city is selected, toggle the filter off
                ACE_GUARD (ACE_Thread_Mutex, mon, m_entityManager->m_vectorLock);

                if (m_entityManager->m_tracker)
                {
                    m_entityManager->m_tracker->m_filterActive = !m_entityManager->m_tracker->m_filterActive;
                    SIM_LOG (LM_INFO, m_entityManager->m_tracker->m_filterActive ? "Filter re-enabled" : "Filter disabled");
                }
            }
        }
        else if (event->key() == Qt::Key_Escape)
        {
            close();
        }
        else if (event->key() == Qt::Key_I)
        {
            // 1. Fetch your active city/filter anchor point target vector
            QVector3D targetLocation = m_entityManager->m_tracker->m_filterAnchor;

            // If no filter anchor is currently active, default target to the North Pole
            if (!m_entityManager->m_tracker->m_filterActive) {
                targetLocation = QVector3D (0.0f, 1.0f, 0.0f);
            }

            /// =========================================================================
            // VERIFIED SPHERICAL LAUNCH SCATTER PIPELINE
            // =========================================================================
            // Establish the baseline high-altitude threat ceiling in deep space (2.5x planet radius)
            float launchAltitude = Globe::globeRadius * 2.0f;
            QVector3D launchOrigin = targetLocation.normalized() * launchAltitude;

            // Fetch a thread-safe random unit vector using your centralized Utility class
            QVector3D scatterOffset = Utility::randomSphericalVector();
            
            // Scale the scatter radius width so threats span a broad tactical grid cone
            float scatterRadius = Globe::globeRadius * 0.75f;
            
            // Apply the directional drift math to randomize the starting launch origin
            launchOrigin += (scatterOffset * scatterRadius);

            // 2. INJECT THE RANDOM COORDINATES INTO THE GPU DRIVEN PIPELINE
            // We pass the unique launch position and the city target vector straight down
            m_entityManager->injectGpuThreat (launchOrigin, targetLocation);            
        }
        else
        {
            // Pass unhandled inputs to the standard QOpenGLWidget base handler
            QOpenGLWidget::keyPressEvent (event);
        }

        updateStatus();
    }

    // Transform mouse handlers

    void MyGLWidget::wheelEvent (QWheelEvent *event)
    {
        // EXTRACT THE DIRECT VERTICAL SCROLL VALUE
        // event->angleDelta().y() returns positive for rolling up, negative for rolling down.
        // We fetch this as a standard float parameter.
        float wheelDelta = static_cast<float> (event->angleDelta().y());

        // Scale the zoom parameter relative to your tracking limits
        Globe::m_zoom -= (wheelDelta * ZOOMSPEEDFACTOR);

        // =========================================================================
        // 3. CLEAN CONFIGURATION CLAMP FENCES (NO MAGIC NUMBERS)
        // =========================================================================
        // Uses the established, hard-checked configuration constants
        if (Globe::m_zoom < ZOOM_CLAMP)
        {
            Globe::m_zoom = ZOOM_CLAMP; // Stops lens from clipping the 1.0 terrain mesh
        } 
        else if (Globe::m_zoom > MAXPULLBACKLIMIT)
        {
            Globe::m_zoom = MAXPULLBACKLIMIT; // Caps maximum pull-back range limits
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
        Globe::m_zoom = ::Config::getInstance().DEFAULT_ZOOM;
        Globe::m_offset = ::Config::getInstance().DEFAULT_OFFSET;
        Globe::m_liveOffset = ::Config::getInstance().DEFAULT_LIVEOFFSET;
        Globe::m_liveTilt = ::Config::getInstance().DEFAULT_TILT;
        Globe::m_rotation = ::Config::getInstance().DEFAULT_ROTATION;
        Globe::m_ambientLevel = ::Config::getInstance().DEFAULT_AMBIENT;

        SIM_LOG (LM_INFO, "Globe reset to default position");
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

    bool MyGLWidget::registerShader_legacy (const QString& name, const QString& vFile, const QString& fFile)
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

    bool MyGLWidget::registerShader (const QString& name, const QString& vFile, const QString& fFile)
    {
        // SAFE PARSER: Loads and enforces strict 32-bit (4-byte) alignment for SPIR-V
        auto loadSPIRV = [](const QString& filePath, std::vector<uint32_t>& buffer) -> bool
        {
            std::ifstream file (filePath.toStdString(), std::ios::binary | std::ios::ate);

            if (!file.is_open()) return false;

            std::streamsize sizeInBytes = file.tellg();

            if (sizeInBytes <= 0 || (sizeInBytes % 4) != 0)
            {
                // SPIR-V specification requires bytecode to be a strict multiple of 4 bytes
                return false; 
            }

            file.seekg (0, std::ios::beg);
            
            // FIXED SAFETY PAD: Calculate how many extra alignment bytes are needed
            std::vector<char> rawBytes (sizeInBytes);
            file.read (rawBytes.data(), sizeInBytes);

            size_t paddedSize = static_cast<size_t> (sizeInBytes);
            while ((paddedSize % 4) != 0)
            {
                rawBytes.push_back ('\0'); // Append trailing null vectors until exactly 4-byte aligned
                paddedSize++;
            }

            size_t wordCount = paddedSize / 4;
            buffer.resize (wordCount);
            std::memcpy (buffer.data(), rawBytes.data(), paddedSize);

            return file.good();
        };

        // 1. DETERMINE SIMULATION TYPE
        // If the fragment path is empty, we are handling a standalone Compute Shader!
        bool isComputeShader = fFile.isEmpty();

        // 1. READ VECTOR DATA INTO WORD-ALIGNED STORAGE BUFFERS
        std::vector<uint32_t> vertBin, fragBin;

        if (!loadSPIRV (vFile, vertBin))
        {
            SIM_LOG (LM_CRITICAL, QString ("CRITICAL: SPIR-V file unaligned or missing: %1 or %2")
                     .arg (vFile)
                     .arg (fFile)
                    );

            return false;
        }

        //Only load the secondary file if this is a graphics pipeline
        if (!isComputeShader)
        {
                if (!loadSPIRV (fFile, fragBin))
                {
                    SIM_LOG (LM_CRITICAL, QString("CRITICAL: SPIR-V fragment file missing or unaligned: %1")
                             .arg (fFile)
                            );

                    return false;
                }
            }

        // 2. EXTRA SAFETY: Verify modern core extensions are fully exposed by Mesa/XCB
        if (!GLAD_GL_ARB_gl_spirv)
        {
            SIM_LOG (LM_CRITICAL, "CRITICAL: The graphics driver context lacks SPIR-V binary execution support!");
            return false;
        }

        // 2. CREATE NATIVE HARDWARE SHADER HANDLES
        // Route to GL_COMPUTE_SHADER or GL_VERTEX_SHADER based on your type flag
        GLuint primaryShaderNum = ::glCreateShader (isComputeShader ? GL_COMPUTE_SHADER : GL_VERTEX_SHADER);
        GLuint fragShaderNum    = isComputeShader ? 0 : ::glCreateShader (GL_FRAGMENT_SHADER);


        // Explicit standard fallback hex token override for safety
        const GLenum SPIRV_BINARY_FORMAT = 0x9551; 

        // 3. UPLOAD RAW BAYTECODE PAYLOADS
        ::glShaderBinary (1, &primaryShaderNum, 
                          SPIRV_BINARY_FORMAT,
                          vertBin.data(),
                          static_cast<GLsizei>(vertBin.size() * 4)
                         );
        ::glSpecializeShader (primaryShaderNum, "main", 0, nullptr, nullptr);

        if (!isComputeShader)
        {
            ::glShaderBinary (1, &fragShaderNum,
                              SPIRV_BINARY_FORMAT,
                              fragBin.data(),
                              static_cast<GLsizei> (fragBin.size() * 4)
                             );
            ::glSpecializeShader (fragShaderNum, "main", 0, nullptr, nullptr);
        }

        // Validate shader specialization success
        GLint primaryCompiled = GL_FALSE, fragCompiled = GL_TRUE;
        ::glGetShaderiv (primaryShaderNum, GL_COMPILE_STATUS, &primaryCompiled);

        if (!isComputeShader)
        {
            ::glGetShaderiv (fragShaderNum, GL_COMPILE_STATUS, &fragCompiled);
        }

        if (primaryCompiled == GL_FALSE || fragCompiled == GL_FALSE)
        {
            SIM_LOG (LM_CRITICAL, QString ("CRITICAL: SPIR-V Specialization failed for target program: %1").arg (name));
            ::glDeleteShader (primaryShaderNum);

            if (!isComputeShader) ::glDeleteShader (fragShaderNum);

            return false;
        }

        // 4. LINK INTO YOUR SHADER PROGRAM CACHE
        QOpenGLShaderProgram* prog = new QOpenGLShaderProgram (this);
        GLuint rawProgramId = prog->programId();

        ::glAttachShader (rawProgramId, primaryShaderNum);

        if (!isComputeShader)
        {
            ::glAttachShader (rawProgramId, fragShaderNum);
        }
        
        ::glLinkProgram(rawProgramId);

        GLint linkStatus = 0;
        ::glGetProgramiv (rawProgramId, GL_LINK_STATUS, &linkStatus);

        ::glDeleteShader (primaryShaderNum);

        if (!isComputeShader) ::glDeleteShader (fragShaderNum);

        if (linkStatus == GL_FALSE)
        {
            SIM_LOG (LM_CRITICAL, QString ("CRITICAL: Program link failed for target pipeline: %1.").arg (name));
            delete prog;

            return false;
        }

        // Cache the fully optimized pre-compiled target program securely
        Globe::m_shaders.insert (name, prog);

        return true;
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

        QDir dir (::Config::getInstance().DATA_DIR_PATH);
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

    bool MyGLWidget::allocateSimulationSSBO (int totalEntities)
    {
        this->makeCurrent();

        if (::glGenBuffers == nullptr || ::glBindBuffer == nullptr || ::glMapBufferRange == nullptr)
        {
            SIM_LOG (LM_CRITICAL, "CRITICAL: allocateSimulationSSBO failed. GLAD 2 pointers are NULL!");
            return false;
        }

        // =========================================================================
        // 1. ALLOCATE SATELLITE SSBO CONTAINER (BINDING SLOT 0)
        // =========================================================================
        ::glGenBuffers (1, &m_ssboHardwareId);

        if (m_ssboHardwareId == 0)
        {
            SIM_LOG (LM_CRITICAL, "SSBO ALLOCATION FAILURE: Driver failed to generate ID for Satellite Buffer.");
            return false;
        }

        ::glBindBuffer (GL_SHADER_STORAGE_BUFFER, m_ssboHardwareId);

        GLsizeiptr bufferSize = totalEntities * sizeof (DataObjects::GpuEntityData);

        // PERSISTENT STORAGE FLAGS:
        // GL_MAP_WRITE_BIT: Thread blocks can write directly to this structure
        // GL_MAP_PERSISTENT_BIT: Pointer remains valid continuously across frames without unmapping
        // GL_MAP_COHERENT_BIT: Writes are automatically made visible to the GPU instantly
        GLbitfield storageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT;
        GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        
        ::glBufferStorage (GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, storageFlags);

        // Map the GPU memory permanently into a CPU pointer address
        m_persistentBufferPtr = reinterpret_cast<DataObjects::GpuEntityData*>(
                                 ::glMapBufferRange (GL_SHADER_STORAGE_BUFFER, 0, bufferSize, mapFlags)
                                );

        if (m_persistentBufferPtr == nullptr)
        {
            GLenum error = ::glGetError();

            SIM_LOG (LM_CRITICAL, QString ("CRITICAL: Satellite Map range failed. Driver code: 0x%1")
                     .arg (error, 0, 16)
                    );

            return false;
        }

        // Connect this buffer permanently to Global Layout Binding slot 0
        ::glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 0, m_ssboHardwareId);
        ::glBindBuffer (GL_SHADER_STORAGE_BUFFER, 0);

        SIM_LOG (LM_INFO, QString ("Zero-Copy Persistent SSBO Initialized. Mapped %1 bytes to GPU slot 0.")
                 .arg (bufferSize)
                );
        
        // Pass the pointer to the EntityManager so the 32 threads can see it
        m_entityManager->setGpuBufferPointer (m_persistentBufferPtr);

        // =========================================================================
        // 2. ALLOCATE MISSILE TRAJECTORY SSBO CONTAINER (BINDING SLOT 1)
        // =========================================================================
        ::glGenBuffers (1, &m_trajectorySsboId);

        if (m_trajectorySsboId == 0)
        {
            SIM_LOG (LM_CRITICAL, "SSBO ALLOCATION FAILURE: Driver failed to generate ID for Missile Buffer.");
            return false;
        }

        ::glBindBuffer (GL_SHADER_STORAGE_BUFFER, m_trajectorySsboId);

        // Sizing for MAX_MISSILES (default is 1000) active missiles, each containing a 64-vertex smooth rendering curve path
        GLsizeiptr trailBufferSize = ::Config::getInstance().MAX_MISSILES * 64 * sizeof (QVector4D); 
        GLbitfield trailFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT;

        if (trailBufferSize <= 0)
        {
            SIM_LOG (LM_CRITICAL, "SSBO FATAL: Calculated missile trajectory trail buffer size is invalid!");
            return false;
        }

        ::glBufferStorage (GL_SHADER_STORAGE_BUFFER, trailBufferSize, nullptr, trailFlags);

        m_persistentTrailPtr = reinterpret_cast<DataObjects::PathVertex*>(
            ::glMapBufferRange (GL_SHADER_STORAGE_BUFFER, 0, trailBufferSize, mapFlags)
        );

        if (m_persistentTrailPtr == nullptr)
        {
            GLenum error = ::glGetError();

            SIM_LOG (LM_CRITICAL, QString("SSBO ALLOCATION FAILURE: Missile Map range failed. Driver code: 0x%1")
                    .arg (error, 0, 16)
                   );

            return false;
        }

        if (m_trajectorySsboId > 0)
        {
            ::glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 1, m_trajectorySsboId);
        }
        else
        {
            SIM_LOG (LM_CRITICAL, "SSBO FATAL: m_trajectorySsboId is corrupted or empty right before binding pass!");
            return false;
        }

        ::glBindBuffer (GL_SHADER_STORAGE_BUFFER, 0);

        SIM_LOG (LM_INFO, QString ("Zero-Copy Persistent SSBO Initialized. Mapped %1 bytes to GPU slot 1.")
                 .arg (trailBufferSize)
                );

        m_entityManager->setGpuTrailPointer (m_persistentTrailPtr);

        SIM_LOG (LM_INFO, "Exit allocateSimulationSSBO()");

        return true;
    }


    void MyGLWidget::renderSatellitePoints_legacy (const QMatrix4x4& mvpMatrix)
    {
        size_t entityCount = m_entityManager->getEntities().size();

        if (entityCount == 0)
        {
            SIM_LOG (LM_DEBUG, "No entities detected");
            return;
        }

        float currentGlobeRadius = ::Config::getInstance().DEFAULT_RADIUS;
        float glDetectionRange   = m_entityManager->m_tracker->m_detectionRange - currentGlobeRadius;

        if (this->setActiveShader ("Satellites"))
        {
            m_program->bind();
            
            // Pass your hardcoded uniform locations directly
            m_program->setUniformValue (0, mvpMatrix);                                    // layout(location = 0)
            m_program->setUniformValue (7, 1.0f, 0.0f, 1.0f, 1.0f);                       // layout(location = 7) Magenta
            m_program->setUniformValue (6, m_entityManager->m_tracker->m_filterActive);   // layout(location = 6)
            
            if (m_entityManager->m_tracker->m_filterActive)
            {
                m_program->setUniformValue  (4, m_entityManager->m_tracker->m_filterAnchor); // layout(location = 4)
                m_program->setUniformValue (5, glDetectionRange);                            // layout(location = 5)
            }

            ::glEnable (GL_PROGRAM_POINT_SIZE); 
            ::glEnable (GL_BLEND); 
            ::glBlendFunc (GL_SRC_ALPHA, GL_ONE); // Glow
            ::glDepthFunc (GL_LEQUAL);

            // Re-assert SSBO slot attachment
            ::glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 0, m_ssboHardwareId);

            // =====================================================================
            // MESA SPECIFICATION SAFETY FIX
            // =====================================================================
            // Bind the empty dummy state container. This completely satisfies 
            // Mesa's Core Profile validation rules and prevents GL_INVALID_OPERATION!
            ::glBindVertexArray (m_dummyVaoId);

            // ZERO-COPY ACCELERATION DRAW CALL:
            // No PCIe copies, no CPU synchronization stalls.
            // The vertex shader reads positions directly out of the shared VRAM memory space.
            ::glDrawArrays (GL_POINTS, 0, static_cast<GLsizei> (entityCount));

            // Restore context state hygiene smoothly
            ::glBindVertexArray (0);
            ::glDisable (GL_BLEND);
            m_program->release();
        }
    }

    void MyGLWidget::renderSatellitePoints (const QMatrix4x4& mvpMatrix)
    {
        // Fetch total capacity allocation matching your MAX_OBJECTS config setting
        int totalSimulationCap = ::Config::getInstance().MAX_OBJECTS;

        // =========================================================================
        // STEP 1: DISPATCH GPU COMPUTE PHYSICS
        // =========================================================================
        if (this->setActiveShader("PhysicsEngine"))
        {
            int totalSimulationCap = ::Config::getInstance().MAX_OBJECTS;

            if (this->setActiveShader ("PhysicsEngine"))
            {
                m_program->bind();
                
                // 1. Compute your real-world Earth core gravity constant relative to your scale factor
                // Real earth standard gravitational parameter (μ) = 398600.4418 km^3/s^2
                double earthMu = 398600.4418;
                
                // Convert to your current GL spatial universe dimensions
                // Accel changes by scaleFactor cubed because volume elements scale by r^3
                float glGravityConstant = static_cast<float>(earthMu) * std::pow (Globe::glScaleFactor, 3.0f);

                // 2. Inject parameters safely to your hardcoded locations
                m_program->setUniformValue (0, this->m_masterDeltaTimeSec);   // location 0
                m_program->setUniformValue (1, Globe::globeRadius);     // location 1
                m_program->setUniformValue (2, glGravityConstant);      // location 2

                ::glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ssboHardwareId);

                int workGroupsX = (totalSimulationCap + 63) / 64;
                ::glDispatchCompute (workGroupsX, 1, 1);

                ::glMemoryBarrier (GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
                m_program->release();
            }
        }

        // =========================================================================
        // STEP 2: DRAW CALLS (SATELLITES & MISSILE POINT TIP INJECTIONS)
        // =========================================================================
        if (this->setActiveShader ("Satellites"))
        {
            m_program->bind();
            m_program->setUniformValue (0, mvpMatrix);                                            // location 0
            m_program->setUniformValue (7, 1.0f, 0.0f, 1.0f, 1.0f);                        // location 7 (Magenta base)
            m_program->setUniformValue (6, m_entityManager->m_tracker->m_filterActive);   // location 6

            if (m_entityManager->m_tracker->m_filterActive)
            {
                float glDetectionRange = m_entityManager->m_tracker->m_detectionRange - Globe::globeRadius;
                m_program->setUniformValue (4, m_entityManager->m_tracker->m_filterAnchor); // location 4
                m_program->setUniformValue (5, glDetectionRange);                           // location 5
            }

            ::glEnable (GL_PROGRAM_POINT_SIZE); 
            ::glEnable (GL_BLEND); 
            ::glBlendFunc (GL_SRC_ALPHA, GL_ONE); 
            ::glDepthFunc (GL_LEQUAL);

            ::glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 0, m_ssboHardwareId);
            ::glBindVertexArray (m_dummyVaoId);

            // Draw everything inside your dynamic asset array in one single high-speed pass
            ::glDrawArrays (GL_POINTS, 0, totalSimulationCap);

            ::glBindVertexArray (0);
            ::glDisable (GL_BLEND);
            m_program->release();
        }
    }

    void MyGLWidget::renderMissileArcs (const QMatrix4x4& mvpMatrix)
    {
        // 1. COMPUTE TOTAL AVAIALABLE WEAPON ALLOCATION SLOTS
        int totalSimulationCap = ::Config::getInstance().MAX_OBJECTS;
        int tacticalStartSlot  = ::Config::getInstance().MAX_SAT_BUFF_SZ;
        
        int totalMissileSlots = totalSimulationCap - tacticalStartSlot;

        if (totalMissileSlots <= 0) return;

        // 2. BIND PIPELINE INFRASTRUCTURE
        if (this->setActiveShader ("MissilePaths"))
        {
            m_program->bind();
            
            // Pass uniforms directly to their explicit location indices
            m_program->setUniformValue (0, mvpMatrix);              // layout(location = 0)
            m_program->setUniformValue (4, 0.0f, 0.8f, 1.0f, 1.0f); // layout(location = 4) Electric Blue
            m_program->setUniformValue (3, tacticalStartSlot);      // layout(location = 3)

            // =====================================================================
            // UNIFIED FILTER DATA INJECTION
            // =====================================================================
            bool isFilterOn = m_entityManager->m_tracker->m_filterActive;
            m_program->setUniformValue (6, isFilterOn); // layout(location = 6)
            
            if (isFilterOn)
            {
                float glDetectionRange = m_entityManager->m_tracker->m_detectionRange - Globe::globeRadius;
                
                // Inject to your layout locations 4 and 5 inside the vertex shader
                m_program->setUniformValue (4, m_entityManager->m_tracker->m_filterAnchor); // location 4
                m_program->setUniformValue (5, glDetectionRange);                           // location 5
            }

            ::glEnable (GL_BLEND);
            ::glBlendFunc (GL_SRC_ALPHA, GL_ONE); 
            ::glDepthFunc (GL_LEQUAL);
            ::glLineWidth (1.0f); 

            // Bind your primary integrated data block straight to global slot 0
            ::glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 0, m_ssboHardwareId);
            ::glBindVertexArray (m_dummyVaoId);

            // =====================================================================
            // OPTIMIZED INSTANCED DRAW COMMAND (ZERO CPU STEPS)
            // =====================================================================
            // This tells the GPU to evaluate all allocated weapon slots in parallel.
            // It bypasses the old driver bottleneck, keeping your startup speed unthrottled.
            ::glDrawArraysInstanced (GL_LINE_STRIP, 0, 64, totalMissileSlots);

            ::glBindVertexArray (0);
            ::glDisable (GL_BLEND);
            m_program->release();
        }
    }

} // namspace SimCore
