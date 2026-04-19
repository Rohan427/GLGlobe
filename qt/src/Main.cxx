#include "LegacyGLApp.hxx"

#include "MyGLWidget.hxx"
#include "MainWindow.hxx"

int main (int argc, char *argv[])
{
    // Force X11/XCB to ensure compatibility profiles work on RHEL 10
    qputenv ("QT_QPA_PLATFORM", "xcb");

    // Tell Mesa to be careful with threading (Optional, but safe for AMD)
    qputenv ("mesa_glthread", "false"); 

    QSurfaceFormat fmt;
    fmt.setVersion (4, 3);
    fmt.setProfile (QSurfaceFormat::CoreProfile);
//    fmt.setProfile (QSurfaceFormat::CompatibilityProfile); // Allows old + new
//    fmt.setDepthBufferSize (24);
    QSurfaceFormat::setDefaultFormat (fmt);

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy (Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication a (argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
