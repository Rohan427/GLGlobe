#pragma once

#ifndef MYGLWIDGET_HXX
#define MYGLWIDGET_HXX

#include "Utility.hxx"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <array>
#include <string>
#include <map>
#include <vector>
#include <QMouseEvent> // Fixes the "incomplete type" error for mouse
#include <QKeyEvent>   // Fixes it for keyboard
#include <QDebug>      // Required for qDebug()
#include <QOpenGLWidget>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLBuffer>
#include <QElapsedTimer>
#include <QImageReader>
#include <QDateTime>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QtMath>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

#include "Tle.h"
#include "CoordGeodetic.h"
#include <memory>
#include "EntityManager.hxx"
#include "Satellite.hxx"
#include "CelesTrakSource.hxx"
#include "FontManager.hxx"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace libsgp4;

static const float ZOOM_CLAMP = 0.165f;
static const float MAXPULLBACKLIMIT = 4.0f;

// APPLY A CONSTANT DYNAMIC PROGRESSION MULTIPLIER
// Instead of using arbitrary hardware divisions, we use a single clear, 
// adjustable scalar variable to map mouse ticks smoothly to zoom intervals.
static const float ZOOMSPEEDFACTOR = 0.0003f;

enum TextureIDs
{
    NOTEXTURE,
    PLAIN_EARTH,        // NE II png
    EARTH_CL8K_DAY,     // NE III, clouds + ice + shadows 8K
    EARTH_CL16K_DAY,    // NE III, clouds + ice + shadows 16K
    EARTH_NC8K_DAY,     // NE III, ice + shadows 8K
    EARTH_NC16K_DAY,    // NE III, ice + shadows 16K
    EARTH8K_DAY,        // NE III, 8k
    EARTH16K_DAY,       // NE III, 16K
    EARTH8K_NIGHT,      // NE III, Night + ice 8K
    EARTH16K_NIGHT,     // NE III, Night + ice 16K
    EARTH8K_BUMP,       // NE III, bump map 8K
    EARTH16k_BUMP,       //NE III, bump map 16K
    TEXTURE_END
};

namespace SimCore
{
    class MyGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_3_Core 
    {
        private:
            Q_OBJECT

            // Critical for all simulation timing
            QElapsedTimer m_frameTimer;
            float m_masterDeltaTimeSec = 0.001f; // Class-scoped master time reference variable

            GLuint textureID = 0;
            GLuint dayTextureID = 0;
            GLuint nightTextureID = 0;
            GLuint bumpTextureID = 0;
            GLuint fontTexture = 0;
            QOpenGLVertexArrayObject m_vao;
            QOpenGLShaderProgram* m_program;
            QOpenGLBuffer m_vbo; // The globe

            GLuint m_dummyVaoId = 0; // Modern spec compatibility state container

            GLuint m_ssboHardwareId = 0;
            DataObjects::GpuEntityData* m_persistentBufferPtr = nullptr;

            // TLE objects
            std::unique_ptr<SGP4> m_issPropagator;
            QVector3D m_issPos;
            QElapsedTimer m_satTimer;
            QVector3D m_lastIssPos;
            EntityManager* m_entityManager = nullptr;
            Globe::FontManager* m_fontManager;

            // Transform variables to track state (mouse)
            QPoint m_lastMousePos;

            QString m_currentStatusString; // To store the "Zoom/Rot/Pos" text

            // For storing sPHere vertex data
            std::vector<float> m_sphereVertices;

            // Track window aspect ratio
            float aspect;

            // Mouse sensitivity
            float sensitivity = 5.0f; // Adjust to feel
            float rotSensitivity = 0.2f;

            Network::CelesTrakSource* m_satelliteSource;

            GLuint m_cityVao;
            GLuint m_cityVbo;

            GLuint m_fontVao;
            GLuint m_fontVbo;

            GLuint m_sensorVao;

            bool m_hasCleanedUp = false;

            /*********************** Functions *******************/

            void initCapitals (QString filename);
            QVector3D latLonToXYZ (float lat, float lon, float radius);
            bool allocateSimulationSSBO (int totalEntities);
            void renderSatellitePoints (const QMatrix4x4& mvpMatrix); // Everything calcualted on GPU
            void renderMissileArcs (const QMatrix4x4& mvpMatrix);
            void releaseSimulationSSBO();
            void restartFullSimulation();

        public:
            // This constructor is required to use the widget in a layout
            explicit MyGLWidget (QWidget* parent = nullptr) : QOpenGLWidget (parent) 
            {
                setFocusPolicy (Qt::StrongFocus);
            }

            ~MyGLWidget() 
            {
                if (m_entityManager)
                {
                    m_entityManager->stop(); // Signal ACE threads to exit
                    m_entityManager->wait(); // Wait for threads to finish
                    delete m_entityManager;
                }

                cleanupGL();
            }

            EntityManager* getEntityManager()
            {
                return m_entityManager;
            }

            void checkLocalCache (const QString& groupKey = "STARLINK");
            void cleanupGL();

        protected:
            // TLE
            void initSatellites();

            void initializeGL() override;
            void paintGL() override;

            // Input Handlers
            void wheelEvent (QWheelEvent *event) override;
            void mouseMoveEvent (QMouseEvent *event) override;
            void mousePressEvent (QMouseEvent *event) override;
            void keyPressEvent (QKeyEvent *event) override;
            void resizeGL (int w, int h) override;
            void resizeEvent (QResizeEvent* event) override;

            // Helper to load and link single shaders
            bool initShader (QOpenGLShaderProgram* program, const QString& vPath, const QString& fPath);
            bool initComputeShader (QOpenGLShaderProgram* program, const QString& cPath);

            // Helpers to add a new shader to the library

            // Load shaders containing GLSL source
            bool registerShader_legacy (const QString& name, const QString& vFile, const QString& fFile);

            // Load shaders containing SPIR-V bytecode
            bool registerShader (const QString& name, const QString& vFile, const QString& fFile);

            // Selects a set of shader programs to activate from the cache
            bool setActiveShader (const QString& name);

            void initializeGlobePosition();

            QVector3D calculateSunDirection();

            // Generate a sphere
            void generateSphere (float radius, int sectors, int stacks);

            void renderMissileHistoryPoints (const QMatrix4x4& mvp);
            void renderRangeRings();

        public slots:
            void resetView()
            {
                initializeGlobePosition();
                updateStatus(); // Repaints AND updates the UI
            }

            void setSpinOffset (double val)
            {
                Globe::m_liveOffset = (float)val;
                updateStatus();
            }

            void setAxialTilt (double val)
            {
                Globe::m_liveTilt = (float)val;
                updateStatus();
            }

            void setAmbientLevel (double val)
            {
                Globe::m_ambientLevel = (float)val;
                updateStatus();
            }

            void setFontSize (int size)
            {
                Globe::m_fontSize = size;
                update();
            }

            void toggleCities (bool visible)
            {
                Globe::m_showCities = visible;
                update();
            }

        signals:
            void cameraChanged (QString status);

            public:
                // Update existing events to emit this signal
                void updateStatus()
                {
                    m_currentStatusString = QString (
                                                     "ZOOM: %1\n"
                                                     "ROT:  %2, %3\n"
                                                     "POS:  %4, %5\n"
                                                     "Tilt: %6\n"
                                                     "Spin: %7\n"
                                                    )
                                                     .arg (Globe::m_zoom, 8, 'f', 2, QChar (' '))// 8 chars total width
                                                     .arg (Globe::m_rotation.x(), 8, 'f', 1, QChar (' '))
                                                     .arg (Globe::m_rotation.y(), 8, 'f', 1, QChar (' '))
                                                     .arg (Globe::m_offset.x(), 8, 'f', 2, QChar (' '))
                                                     .arg (Globe::m_offset.y(), 8, 'f', 2, QChar (' '))
                                                     .arg (Globe::m_liveTilt, 6, 'f', 2, QChar (' '))
                                                     .arg (Globe::m_liveOffset, 6, 'f', 2, QChar (' '));
                    update();
                }
    };
} // namespace SimCore

#endif // MYGLWIDGET_HXX
