#include "LegacyGLApp.hxx"

#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_3_Core>
#include "MainWindow.hxx"
#include <ace/Signal.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <atomic>

#define DEBUG false

static std::atomic<bool> g_shutdownRequested{false};
static std::atomic<int>  g_lastSignal{0};


static void signalHandler (int sig)
{
    const char* sigName = "unknown";
    switch (sig)
    {
        case SIGINT:  sigName = "SIGINT";  break;
        case SIGTERM: sigName = "SIGTERM"; break;
        case SIGHUP:  sigName = "SIGHUP";  break;
        case SIGSEGV: sigName = "SIGSEGV"; break;
        case SIGABRT: sigName = "SIGABRT"; break;
        case SIGTSTP: sigName = "SIGTSTP"; break;
        case SIGBUS: sigName = "SIGABRT"; break;
        case SIGILL: sigName = "SIGBUS"; break;
        case SIGFPE: sigName = "SIGFPE"; break;
            // Add more as needed
    }

    ACE_DEBUG ((LM_CRITICAL, ACE_TEXT ("Received signal %s (%d) - shutting down...\n"), sigName, sig));

    if (sig == SIGSEGV || sig == SIGABRT)
    {
        void* frames[64];
        int n = ::backtrace (frames, 64);
        char** symbols = ::backtrace_symbols (frames, n);

        ACE_DEBUG ((LM_CRITICAL, ACE_TEXT("--- BACKTRACE (%d frames) ---\n"), n));

        for (int i = 0; i < n; ++i)
        {
            ACE_DEBUG ((LM_CRITICAL, ACE_TEXT ("  [%d] %s\n"), i, symbols[i]));
        }

        ACE_DEBUG ((LM_CRITICAL, ACE_TEXT ("--- END BACKTRACE ---\n")));

        ::free (symbols);

        // Optional: let the default handler dump a core
        ::signal (sig, SIG_DFL);
        ::raise (sig);
    }

    g_lastSignal.store (sig, std::memory_order_relaxed);
    g_shutdownRequested.store (true, std::memory_order_relaxed);
}

using namespace SimCore;

//int main (int argc, char *argv[])
int ACE_TMAIN (int argc, ACE_TCHAR *argv[])
{
    // Register signal handler
    ACE_Sig_Action sa (signalHandler);
    sa.register_action (SIGINT);
    sa.register_action (SIGTERM);
    sa.register_action (SIGHUP);
    sa.register_action (SIGTSTP);
    sa.register_action (SIGBUS);
    sa.register_action (SIGILL);
    sa.register_action (SIGFPE);
    sa.register_action (SIGABRT);
    sa.register_action (SIGSEGV);

    // Set ACE to show: Time | Severity | Thread ID | Message
    ACE_Log_Msg::instance()->open (argv[0], ACE_Log_Msg::STDERR); // | ACE_Log_Msg::LOGGER);
    ACE_Log_Msg::instance()->priority_mask (/*LM_DEBUG | */ LM_INFO | LM_ERROR | LM_CRITICAL, ACE_Log_Msg::PROCESS);

    ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Starting up"));

    // Must load application config parameters first
    ::Config::getInstance().init ("configuration.json");

#if DEBUG
    std::cout << "DEFAULT_LIVEOFFSET " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_LIVEOFFSET << std::endl;
    std::cout << "DEFAULT_TILT " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_TILT << std::endl;
    std::cout << "DEFAULT_ROTATION X " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_ROTATION.x() << std::endl;
    std::cout << "DEFAULT_ROTATION Y " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_ROTATION.y() << std::endl;
    std::cout << "DEFAULT_AMBIENT " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_AMBIENT << std::endl;
    std::cout << "DEFAULT_RADIUS " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_RADIUS << std::endl;
    std::cout << "DEFAULT_SECTORS " << ::Config::getInstance().DEFAULT_SECTORS << std::endl;
    std::cout << "DEFAULT_STACKS " << ::Config::getInstance().DEFAULT_STACKS << std::endl;
    std::cout << "DEFAULT_PERSPECTIVE " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_PERSPECTIVE << std::endl;
    std::cout << "DEFAULT_FONT_SIZE " << ::Config::getInstance().DEFAULT_FONT_SIZE << std::endl;
    std::cout << "DEFAULT_MARKER_SIZE " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_MARKER_SIZE << std::endl;
    std::cout << "DEFAULT_ZOOM " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_ZOOM  << std::endl;
    std::cout << "DEFAULT_OFFSET X " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_OFFSET.x() << std::endl;
    std::cout << "DEFAULT_OFFSET Y " << std::fixed << std::setprecision (5) << ::Config::getInstance().DEFAULT_OFFSET.y() << std::endl;
    std::cout << "LABEL_HEIGHT_OFFSET " << std::fixed << std::setprecision (5) << ::Config::getInstance().LABEL_HEIGHT_OFFSET << std::endl;
    std::cout << "THREAD_SLEEP_TIME " << ::Config::getInstance().THREAD_SLEEP_TIME << std::endl;
    std::cout << "MAX_THREADS " << ::Config::getInstance().MAX_THREADS << std::endl;
    std::cout << "DATA_DIR_PATH " << ::Config::getInstance().DATA_DIR_PATH.toStdString() << std::endl;
    std::cout << "DATA_FILE_SAT_SUFFIX " << ::Config::getInstance().DATA_FILE_SAT_SUFFIX.toStdString() << std::endl;
    std::cout << "CELESTRAK_URL " << ::Config::getInstance().CELESTRAK_URL.toStdString() << std::endl;
    std::cout << "FONT_PATH " << ::Config::getInstance().FONT_PATH.toStdString() << std::endl;
#endif


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
