#pragma once

#include <QMainWindow>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include "MyGLWidget.hxx"

class MainWindow : public QMainWindow
{
    QPlainTextEdit* console;
    MyGLWidget* glViewport;

    public:
        MainWindow()
        {
            // Central container
            QWidget* central = new QWidget (this);
            setCentralWidget (central);
            QHBoxLayout* mainLayout = new QHBoxLayout (central);

            // 1. OpenGL Viewport (Left)
            glViewport = new MyGLWidget (this);
            //mainLayout->addWidget (glViewport, 4); // Stretch factor of 4

            // 2. Sidebar (Right)
            // A. Create a container widget for the sidebar
            QWidget* sidebarContainer = new QWidget(this);
            sidebarContainer->setFixedWidth (450); // Adjust this value to your liking

            // B. Create the vertical layout for this container
            QVBoxLayout* sidebar = new QVBoxLayout (sidebarContainer);

            // C. Buttons
            QPushButton* refreshBtn = new QPushButton ("Refresh Texture", this);
            sidebar->addWidget (refreshBtn);
            QPushButton* resetBtn = new QPushButton ("Reset View", this);
            sidebar->addWidget (resetBtn);

            // D. Status label:
            QLabel* statusLabel = new QLabel ("Zoom: 1.00 | Rot: 0.0, 0.0", this);
            statusLabel->setAlignment (Qt::AlignLeft | Qt::AlignTop);
            statusLabel->setStyleSheet ("font-family: 'DejaVu Sans Mono', 'Courier New', monospace; "
                                        "font-size: 12pt; " // Use 'pt' for high-res scaling
                                        "font-weight: bold; "
                                        "color: #00FF00; "
                                        "background-color: #1A1A1A; "
                                        "padding: 10px; "
                                        "border: 1px solid #333;"
                                       );
            sidebar->addWidget (statusLabel);

            // Connect the widget signal to the label's text slot
            connect (glViewport, &MyGLWidget::cameraChanged, statusLabel, &QLabel::setText);
            
            // E. Console display
            console = new QPlainTextEdit (this);
            console->setReadOnly (true);
            console->setPlaceholderText ("System Console...");
            sidebar->addWidget (console);

            // F. Connect "Refresh" button to the GL logic
            connect (refreshBtn, &QPushButton::clicked, [this]() 
                        {
                            console->appendPlainText ("> Triggering Texture Refresh...");
                            glViewport->update(); 
                        }
                    );

            // G. Connect "Reset" button to the GL logic
            connect (resetBtn, &QPushButton::clicked, glViewport, &MyGLWidget::resetView);
            connect (resetBtn, &QPushButton::clicked, [this]()
                        {
                            console->appendPlainText ("> Viewport transformations reset to default.");
                        }
                    );

            // 3. Add the CONTAINER to the mainLayout instead of just the layout
            mainLayout->addWidget (glViewport, 1);       // OpenGL takes the rest
            mainLayout->addWidget (sidebarContainer, 0); // Sidebar stays 300px

            // 4. Set a larger initial window size
            this->setMinimumSize (1920, 1080); // Start at 1080p size even on 4K
        }
};
