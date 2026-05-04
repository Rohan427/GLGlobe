//#include "LegacyGLApp.hxx"


#include <QOpenGLWidget>
#include <QApplication>
#include <QOpenGLFunctions_4_3_Core>
//#include "MyGLWidget.hxx"
#include "MainWindow.hxx"
#include <ace/Log_Msg.h>

using namespace SimCore;

//int main (int argc, char *argv[])
int ACE_TMAIN (int argc, ACE_TCHAR *argv[])
{
    // Set ACE to show: Time | Severity | Thread ID | Message
    ACE_Log_Msg::instance()->open (argv[0], ACE_Log_Msg::STDERR); // | ACE_Log_Msg::LOGGER);
    ACE_Log_Msg::instance()->priority_mask (LM_DEBUG | LM_INFO | LM_ERROR | LM_CRITICAL, ACE_Log_Msg::PROCESS);

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Starting up"));

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
    fmt.setSamples (16); // 8x MSAA
    QSurfaceFormat::setDefaultFormat (fmt);

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy (Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Initializing Application"));
    QApplication a (argc, argv);

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Initializing Main Window"));
    MainWindow w;
    w.show();

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Starting Application"));
    return a.exec();
}
