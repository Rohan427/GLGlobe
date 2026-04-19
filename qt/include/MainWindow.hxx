#pragma once

#include <QMainWindow>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QShortcut>
#include <QKeySequence>
#include "MyGLWidget.hxx"

class MainWindow : public QMainWindow
{
    Q_OBJECT
    QWidget* sidebarContainer;
    QPlainTextEdit* console;
    MyGLWidget* glViewport;

    public:
        MainWindow()
        {
            // Central container
            QWidget* central = new QWidget (this);
            setCentralWidget (central);
            QVBoxLayout* rootLayout = new QVBoxLayout (central); // Vertical to hold toolbar + content

            // Create Top Toolbar (for the toggle button)
            QHBoxLayout* toolbar = new QHBoxLayout();

            // Create a Toggle Button (at the top of the sidebar or in a toolbar)
            QPushButton* toggleBtn = new QPushButton ("« Toggle Sidebar (Tab)", this);
            toggleBtn->setFixedWidth (220);
            toolbar->addWidget (toggleBtn);
            toolbar->addStretch(); // Pushes button to the left
            rootLayout->addLayout (toolbar);

            QHBoxLayout* mainLayout = new QHBoxLayout();
            rootLayout->addLayout (mainLayout);

            // 1. OpenGL Viewport (Left)
            glViewport = new MyGLWidget (this);
            mainLayout->addWidget (glViewport, 1); // Stretch factor of 1

            // 2. Sidebar (Right)
            // A. Initialize the Sidebar Container (CRITICAL: Assign to member variable)
            this->sidebarContainer = new QWidget(this);
            this->sidebarContainer->setFixedWidth (450);
            QVBoxLayout* sidebar = new QVBoxLayout (this->sidebarContainer);

            // C. Sidebar buttons
            QPushButton* refreshBtn = new QPushButton ("Refresh Texture", this);
            sidebar->addWidget (refreshBtn);
            QPushButton* resetBtn = new QPushButton ("Reset View", this);
            sidebar->addWidget (resetBtn);

            /****** Collapsible sidebar ******/
            // Add it to a toolbar or the very top of the layout
            // Putting it in a small top-bar is best for collapsible UIs
            connect (toggleBtn, &QPushButton::clicked, this, &MainWindow::toggleSidebar);

            // Optional: Add a Keyboard Shortcut (the '~' or 'Tab' key)
            QShortcut* shortcut = new QShortcut(QKeySequence (Qt::Key_Tab), this);
            connect (shortcut, &QShortcut::activated, this, &MainWindow::toggleSidebar);

            /****** End Collapsible sidebar ******/

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
            // Also add a rest shortcut key (keyboard H)
            QShortcut* homeKey = new QShortcut (QKeySequence (Qt::Key_H), this);
            connect (homeKey, &QShortcut::activated, glViewport, &MyGLWidget::resetView);

            // 3. Add the CONTAINER to the mainLayout instead of just the layout
            mainLayout->addWidget (glViewport, 1);       // OpenGL takes the rest
            mainLayout->addWidget (sidebarContainer, 0); // Sidebar stays 300px

            // 4. Set a larger initial window size
            this->setMinimumSize (1920, 1080); // Start at 1080p size even on 4K
        }

    public slots:
        void toggleSidebar()
        {
            if (!sidebarContainer) return; // Safety check

            bool hidden = sidebarContainer->isVisible();
            sidebarContainer->setVisible (!hidden);

            // Update the button text if you have a pointer to it (e.g., m_toggleBtn)
            // m_toggleBtn->setText (!hidden ? "« Hide Sidebar" : "Show Sidebar »");

            // Log the action to the console
            console->appendPlainText (!hidden ? "> Sidebar Restored" : "> Sidebar Collapsed (4K View Active)");
        }
};
