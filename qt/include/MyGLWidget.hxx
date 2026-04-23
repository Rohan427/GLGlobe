#pragma once

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


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Constant update while debugging
#include <QTimer>

class MyGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_3_Core 
{
    private:
        Q_OBJECT
        GLuint textureID = 0;
        QOpenGLVertexArrayObject m_vao;
        QOpenGLShaderProgram* m_program;
        QOpenGLBuffer m_vbo;

        // Shader program for compute tasks
        QOpenGLShaderProgram* m_computeProgram;
        QElapsedTimer timer;

        // Transform variables to track state (mouse)
        float m_zoom = 1.0f;
        QVector2D m_rotation; // x = pitch, y = yaw
        QPoint m_lastMousePos;
        QVector2D m_offset;   // for dragging

        QString m_currentStatusString; // To store the "Zoom/Rot/Pos" text

        // For storing shpere vertex data
        std::vector<float> m_sphereVertices;

        // Track window aspect ratio
        float aspect;

        // Mouse sensitivity
        float sensitivity = 5.0f; // Adjust to feel

        // Initial globe settings
        float m_liveOffset = -90.0f;
        float m_liveTilt = 23.5f;

    public:
        // This constructor is required to use the widget in a layout
        explicit MyGLWidget (QWidget* parent = nullptr) : QOpenGLWidget (parent) 
        {}

        void generateSphere (float radius, int sectors, int stacks);
        GLuint loadMapTexture (const QString& filePath);
        void initializeGlobePosition();

    protected:
        void initializeGL() override;
        void paintGL() override;

        // Input Handlers
        void wheelEvent (QWheelEvent *event) override;
        void mouseMoveEvent (QMouseEvent *event) override;
        void mousePressEvent (QMouseEvent *event) override;
        void keyPressEvent (QKeyEvent *event) override;
        void resizeGL (int w, int h) override;

        // Calculate the real-world sun direction
        QVector3D calculateSunDirection();

        // To test basic pipeline with vertex + fragment shaders
        GLuint createSimpleTexture (int w, int h);
        
        // To test compute shader inpipeline
        GLuint createDynamicTexture (int w, int h);

        // Inside the widget for executing compute shader
        void runCompute();

    public slots:
        void resetView()
        {
            /* m_zoom = 1.0f;
            m_offset = QVector2D(0.0f, 0.0f);
            
            // 90 on X brings the Z-axis (Pole) to the Y-axis (Top).
            // 0 on Y ensures we aren't rotated "sideways".
            m_rotation = QVector2D(90.0f, 0.0f);*/

            initializeGlobePosition();

            updateStatus(); // Repaints AND updates the UI
        }

        void setSpinOffset (double val)
        {
            m_liveOffset = val;
            updateStatus();
        }

        void setAxialTilt (double val)
        {
            m_liveTilt = val;
            updateStatus();
        }

    signals:
        void cameraChanged (QString status);

        public:
        // Update your existing events to emit this signal
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
                                                 .arg (m_liveTilt, 6, 'f', 2, QChar (' '))
                                                 .arg (m_liveOffset, 6, 'f', 2, QChar (' '));
                update();
            }
};

