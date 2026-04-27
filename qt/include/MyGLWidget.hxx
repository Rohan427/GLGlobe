#pragma once

//#include "LegacyGLApp.hxx"
#include <iostream>
#include <cstdlib>
#include <array>
#include <string>
#include <map>
#include <QMouseEvent> // Fixes the "incomplete type" error for mouse
#include <QKeyEvent>   // Fixes it for keyboard
#include <QDebug>      // Required for qDebug()
#include <QOpenGLWidget>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions_4_3_Core>
#include <vector>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QElapsedTimer>
#include <QImageReader>
#include <QDateTime>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QtMath>
//#include "MainWindow.hxx"

#include "SGP4.h"
#include "Tle.h"
#include "CoordGeodetic.h"
#include <memory>
#include "Utility.hxx"
#include "Globe.hxx"
#include "EntityManager.hxx"
#include "Satellite.hxx"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace libsgp4;

static const float ZOOM_CLAMP = 0.13f;

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
            GLuint textureID = 0;
            GLuint dayTextureID = 0;
            GLuint nightTextureID = 0;
            GLuint bumpTextureID = 0;
            QOpenGLVertexArrayObject m_vao;
            QOpenGLShaderProgram* m_program;
            QOpenGLBuffer m_vbo;

            // TLE objects
            std::unique_ptr<SGP4> m_issPropagator;
            QVector3D m_issPos;
            QElapsedTimer m_satTimer;
            QVector3D m_lastIssPos;
            EntityManager* m_entityManager = nullptr; 

            // Transform variables to track state (mouse)
            float m_zoom = 1.0f;
            QVector2D m_rotation; // x = pitch, y = yaw
            QPoint m_lastMousePos;
            QVector2D m_offset;   // for dragging

            QString m_currentStatusString; // To store the "Zoom/Rot/Pos" text

            // For storing sPHere vertex data
            std::vector<float> m_sphereVertices;

            // Track window aspect ratio
            float aspect;

            // Mouse sensitivity
            float sensitivity = 5.0f; // Adjust to feel
            float rotSensitivity = 0.2f;

            

            /*********************** Functions *******************/

            void initCapitals (QString filename);
            QVector3D latLonToXYZ (float lat, float lon, float radius);

        public:
            // This constructor is required to use the widget in a layout
            explicit MyGLWidget (QWidget* parent = nullptr) : QOpenGLWidget (parent) 
            {
            }

            ~MyGLWidget() 
            {
                if (m_entityManager)
                {
                    m_entityManager->stop(); // Signal ACE threads to exit
                    m_entityManager->wait(); // Wait for threads to finish
                    delete m_entityManager;
                }
            }

        protected:
            // TLE
            void initSatellites();
            void updateSatellitePhysics (qint64 msecs);


            void initializeGL() override;
            void paintGL() override;

            // Input Handlers
            void wheelEvent (QWheelEvent *event) override;
            void mouseMoveEvent (QMouseEvent *event) override;
            void mousePressEvent (QMouseEvent *event) override;
            void keyPressEvent (QKeyEvent *event) override;
            void resizeGL (int w, int h) override;

            // Helper to load and link shaders
            bool initShader (QOpenGLShaderProgram* program, const QString& vPath, const QString& fPath);
            bool initComputeShader (QOpenGLShaderProgram* program, const QString& cPath);
            bool setActiveShader (const QString& name);

            // Helper to add a new shader to the library
            bool registerShader (const QString& name, const QString& vFile, const QString& fFile);

            // Calculate the real-world sun direction
            QVector3D calculateSunDirection();

            // To test basic pipeline with vertex + fragment shaders
            GLuint createSimpleTexture (int w, int h);

            GLuint loadTexture (std::array<int, 2>& mapSize, const QString& filePath);
            bool loadTextureFiles (std::array<int, 2>& mapSize);

            void initializeGlobePosition();

            // Square Geometry (X, Y, U, V)
            float* createPlane();

            // Plane X, Y, U, V
            float* createLargePlane();

            // Cube with normals X, Y, Z, U, V, NX, NY, NZ (8 floats per vertex)
            float* createNormalCube();

            // X, Y, Z, U, V
            float* createCube();

            // Generate a sphere
            void generateSphere (float radius, int sectors, int stacks);
            
            // To test compute shader inpipeline
            GLuint createDynamicTexture (int w, int h);

            void loadTextures (std::array<int, 2>& mapSize);

            // Inside the widget for executing compute shader
            void runCompute();

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
                                                     .arg (m_zoom, 8, 'f', 2, QChar (' '))// 8 chars total width
                                                     .arg (m_rotation.x(), 8, 'f', 1, QChar (' '))
                                                     .arg (m_rotation.y(), 8, 'f', 1, QChar (' '))
                                                     .arg (m_offset.x(), 8, 'f', 2, QChar (' '))
                                                     .arg (m_offset.y(), 8, 'f', 2, QChar (' '))
                                                     .arg (Globe::m_liveTilt, 6, 'f', 2, QChar (' '))
                                                     .arg (Globe::m_liveOffset, 6, 'f', 2, QChar (' '));
                    update();
                }
    };
} // namespace SimCore
