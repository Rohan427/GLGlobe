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

    public:
        // This constructor is required to use the widget in a layout
        explicit MyGLWidget (QWidget* parent = nullptr) : QOpenGLWidget (parent) 
        {}

        void generateSphere (float radius, int sectors, int stacks);
        GLuint loadMapTexture (const QString& filePath);

    protected:
        void initializeGL() override;
        void paintGL() override;

        // Input Handlers
        void wheelEvent (QWheelEvent *event) override;
        void mouseMoveEvent (QMouseEvent *event) override;
        void mousePressEvent (QMouseEvent *event) override;
        void keyPressEvent (QKeyEvent *event) override;
        void resizeGL (int w, int h) override;

        // To test basic pipeline with vertex + fragment shaders
        GLuint createSimpleTexture (int w, int h);
        
        // To test compute shader inpipeline
        GLuint createDynamicTexture (int w, int h);

        // Inside your widget for executing compute shader
        void runCompute();

    public slots:
        void resetView()
        {
            m_zoom = .57f; // Ideal for test cube camera distance
            m_rotation = QVector2D (20.0f, 45.0f); // Slight tilt looks better in 3D
            m_offset = QVector2D (0.0f, 0.0f);
            updateStatus(); // Repaints AND updates the UI
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
                                                 "POS:  %4, %5"
                                                )
                                                 .arg (m_zoom, 8, 'f', 2, QChar (' '))// 8 chars total width
                                                 .arg (m_rotation.x(), 8, 'f', 1, QChar (' '))
                                                 .arg (m_rotation.y(), 8, 'f', 1, QChar (' '))
                                                 .arg (m_offset.x(), 8, 'f', 2, QChar (' '))
                                                 .arg (m_offset.y(), 8, 'f', 2, QChar (' '));
                update();
            }
};

