#include "MainWindow.hxx"
#include "EntityManager.hxx"


using namespace SimCore;

MainWindow* MainWindow::s_instance = nullptr;

MainWindow::MainWindow (QWidget *parent) : QMainWindow(parent)
{
    s_instance = this;

    // Create the satellitedata source
    satelliteSource = new Network::CelesTrakSource();

    // Central container
    QWidget* central = new QWidget (this);
    setCentralWidget (central);
    QVBoxLayout* rootLayout = new QVBoxLayout (central); // Vertical to hold toolbar + content

    // Create Top Toolbar (for the toggle button)
    QHBoxLayout* toolbar = new QHBoxLayout();


    // Create the Settings Menu
    QMenu* settingsMenu = new QMenu ("Settings", this);
    settingsMenu->setStyleSheet ("QMenu::item { padding: 10px 20px; font-size: 14pt; }");

    // Create the Toggle Action
    QAction* toggleCityAct = new QAction ("Show Cities", this);
    toggleCityAct->setCheckable (true); // Turns it into a checkbox
    toggleCityAct->setChecked (true);    // Default to 'On'

    // Add a Tool Button to the toolbar that opens this menu
    QToolButton* settingsBtn = new QToolButton (this);
    settingsBtn->setText ("Settings");
    settingsBtn->setPopupMode (QToolButton::InstantPopup);
    settingsBtn->setMenu (settingsMenu);
    toolbar->addWidget (settingsBtn);

    // Add the action to the menu
    settingsMenu->addAction (toggleCityAct);

    updateSatsAct = new QAction ("Update Satellite Data", this);
    settingsMenu->addAction (updateSatsAct);



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

    //********** Controls for adjusting globe alignment **********//
    QVBoxLayout* controlLayout = new QVBoxLayout();

    // Spin Offset Control
    // 1. Add the Label to the controlLayout
    QLabel* offsetLabel = new QLabel ("UTC Spin Offset:", this);
    offsetLabel->setStyleSheet ("font-weight: bold; margin-top: 5px;");
    controlLayout->addWidget (offsetLabel);

    // 2. Add the SpinBox to the SAME controlLayout
    QDoubleSpinBox* offsetSpin = new QDoubleSpinBox(this);
    offsetSpin->setRange (-360.0, 360.0);
    offsetSpin->setValue (-90.0);
    controlLayout->addWidget (offsetSpin);

    // Seasonal Tilt Control
    QLabel* tiltLabel = new QLabel ("Axial Tilt (deg):", this);
    tiltLabel->setStyleSheet ("font-weight: bold; margin-top: 5px;");
    controlLayout->addWidget (tiltLabel);

    QDoubleSpinBox* tiltSpin = new QDoubleSpinBox(this);
    tiltSpin->setRange (-30.0, 30.0);
    tiltSpin->setValue (-23.5); 
    controlLayout->addWidget (tiltSpin);

    // Ambient spin control
    QLabel* ambientLabel = new QLabel ("Ambient Light Level:", this);
    ambientLabel->setStyleSheet ("font-weight: bold; margin-top: 5px;");
    controlLayout->addWidget (ambientLabel);

    QDoubleSpinBox* ambientSpin = new QDoubleSpinBox (this);
    ambientSpin->setRange (0.0, 1.0);
    ambientSpin->setSingleStep (0.01);
    ambientSpin->setValue (0.15); // Default value
    controlLayout->addWidget (ambientSpin);

    

    // Connect them to your GL Widget
    connect (offsetSpin, &QDoubleSpinBox::valueChanged, glViewport, &MyGLWidget::setSpinOffset);
    connect (tiltSpin, &QDoubleSpinBox::valueChanged, glViewport, &MyGLWidget::setAxialTilt);
    connect (ambientSpin, &QDoubleSpinBox::valueChanged, glViewport, &MyGLWidget::setAmbientLevel);
    connect (toggleCityAct, &QAction::toggled, glViewport, &MyGLWidget::toggleCities);
//    connect (updateSatsAct, &QAction::triggered, satelliteSource, &Network::BaseDataSource::requestUpdate);

    //********** END Controls (Remove when complete **********//

    // Font control
    controlLayout->addWidget (new QLabel ("Label Font Size:", this));
    QSpinBox* fontSpin = new QSpinBox (this);
    fontSpin->setRange (6, 72);
    fontSpin->setValue (10);
    controlLayout->addWidget (fontSpin);
    connect (fontSpin, &QSpinBox::valueChanged, glViewport, &MyGLWidget::setFontSize);


    sidebar->addLayout (controlLayout);

    /****** Collapsible sidebar ******/
    // Add it to a toolbar or the very top of the layout
    // Putting it in a small top-bar is best for collapsible UIs
    connect (toggleBtn, &QPushButton::clicked, this, &MainWindow::toggleSidebar);

    // Optional: Add a Keyboard Shortcut (the '~' or 'Tab' key)
    QShortcut* shortcut = new QShortcut (QKeySequence (Qt::Key_Tab), this);
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
    // Also add a reset shortcut key (keyboard H)
    QShortcut* homeKey = new QShortcut (QKeySequence (Qt::Key_H), this);
    connect (homeKey, &QShortcut::activated, glViewport, &MyGLWidget::resetView);

    // 3. Add the CONTAINER to the mainLayout instead of just the layout
    mainLayout->addWidget (glViewport, 1);       // OpenGL takes the rest
    mainLayout->addWidget (sidebarContainer, 0); // Sidebar stays 300px

    // 4. Set a larger initial window size
    this->setMinimumSize (1920, 1080); // Start at 1080p size even on 4K


}

MainWindow* MainWindow::instance()
{
    return s_instance;
}

void MainWindow::logMessage (const QString& msg)
{
    if (console)
    {
        // Appends text with a newline and scrolls to bottom
        console->appendPlainText (msg);
    }
}

Network::CelesTrakSource* MainWindow::getSatelliteSource()
{
    return satelliteSource;
}

QAction* MainWindow::getUpdateSatsAct()
{
    return updateSatsAct;
}
