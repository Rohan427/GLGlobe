#pragma once

//#include "LegacyGLApp.hxx"
#include <QMainWindow>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QShortcut>
#include <QKeySequence>
#include <QDoubleSpinBox>
#include <QToolButton>
#include <QMenu>
#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include "MyGLWidget.hxx"
#include "CelesTrakSource.hxx"

namespace SimCore
{
    class MainWindow : public QMainWindow
    {
        Q_OBJECT

        public:
            void logMessage (const QString& msg);

            explicit MainWindow (QWidget *parent = nullptr);
            static MainWindow* instance(); // For global access
            Network::CelesTrakSource* getSatelliteSource();
            QAction* getUpdateSatsAct();

        private:
            static MainWindow* s_instance;
            QWidget* sidebarContainer;
            QPlainTextEdit* console;
            MyGLWidget* glViewport;
            Network::CelesTrakSource* satelliteSource;
            QAction* updateSatsAct; 

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
} // namespace SimCore
