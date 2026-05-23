#include "Utility.hxx"
#include "MainWindow.hxx" // Included safely here in the source file

namespace SimCore
{
    void routeLogToGui (int level, const QString& msg)
    {
        if (SimCore::MainWindow::instance() && (level == LM_INFO || level == LM_ERROR || level == LM_CRITICAL || level == LM_WARNING))
        {
            SimCore::MainWindow::instance()->logMessage(msg);
        }
    }
}
