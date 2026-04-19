#pragma once

#include <QMouseEvent> // Fixes the "incomplete type" error for mouse
#include <QKeyEvent>   // Fixes it for keyboard
#include <QDebug>      // Required for qDebug()
#include <QOpenGLWidget>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions>
#include <vector>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>

// Constant update while debugging
#include <QTimer>

class MyGLWidget : public QOpenGLWidget, protected QOpenGLFunctions 
{
    private:
        Q_OBJECT
        GLuint textureID = 0;
        QOpenGLVertexArrayObject m_vao;
        QOpenGLShaderProgram* m_program;
        QOpenGLBuffer m_vbo;

        // Transform variables to track state (mouse)
        float m_zoom = 1.0f;
        QVector2D m_rotation; // x = pitch, y = yaw
        QPoint m_lastMousePos;
        QVector2D m_offset;   // for dragging

    public:
        // This constructor is required to use the widget in a layout
        explicit MyGLWidget (QWidget* parent = nullptr) : QOpenGLWidget (parent) 
        {}

    protected:
        void initializeGL() override;
        void paintGL() override;

        // Input Handlers
        void wheelEvent (QWheelEvent *event) override;
        void mouseMoveEvent (QMouseEvent *event) override;
        void mousePressEvent (QMouseEvent *event) override;
        void keyPressEvent (QKeyEvent *event) override;

        GLuint createSimpleTexture (int w, int h);

        // Inside your widget for executing compute shader
        void runCompute();

    public slots:
        void resetView()
        {
            m_zoom = 1.0f;
            m_rotation = QVector2D(0.0f, 0.0f);
            m_offset = QVector2D(0.0f, 0.0f);
            updateStatus(); // Repaints AND updates the UI
        }

    signals:
        void cameraChanged (QString status);

        public:
        // Update your existing events to emit this signal
            void updateStatus()
            {
                QString status = QString (
                                          "ZOOM: %1\n"
                                          "ROT:  %2, %3\n"
                                          "POS:  %4, %5"
                                         )
                                          .arg (m_zoom, 8, 'f', 2, QChar (' '))// 8 chars total width
                                          .arg (m_rotation.x(), 8, 'f', 1, QChar (' '))
                                          .arg (m_rotation.y(), 8, 'f', 1, QChar (' '))
                                          .arg (m_offset.x(), 8, 'f', 2, QChar (' '))
                                          .arg (m_offset.y(), 8, 'f', 2, QChar (' '));

                emit cameraChanged(status);
                update();
            }
};

