#include "LegacyGLApp.hxx"

#include "MyGLWidget.hxx"
#include "MainWindow.hxx"

int main (int argc, char *argv[])
{
    // 1. Force Qt to ONLY look in the standard RHEL system plugin directory
    // This stops it from scanning your local build folders and crashing
    qputenv ("QT_PLUGIN_PATH", "/usr/lib64/qt6/plugins");

    // 2. Disable QT_DEBUG_PLUGINS to prevent the verbose factoryloader crash
    qunsetenv ("QT_DEBUG_PLUGINS"); 

    // Force X11/XCB to ensure compatibility profiles work on RHEL 10
    qputenv ("QT_QPA_PLATFORM", "xcb");

    // Tell Mesa to be careful with threading (Optional, but safe for AMD)
    qputenv ("mesa_glthread", "false"); 

    QSurfaceFormat fmt;
    fmt.setVersion (4, 3);
    fmt.setProfile (QSurfaceFormat::CoreProfile);

    // Set MSAA precision
    fmt.setSamples (8); // 8x MSAA
    QSurfaceFormat::setDefaultFormat (fmt);

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy (Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication a (argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
