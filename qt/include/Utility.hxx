#pragma once

#include <QtMath>
#include <QVector3D>
#include <QString>
#include <QDateTime>
#include <ace/Log_Msg.h>

#define SIM_LOG(level, msg) \
    do { \
    QString qmsg = QString(msg); \
    /* Route ONLY high-priority logs to the GUI */ \
        if (MainWindow::instance() && (level == LM_INFO || level == LM_ERROR || level == LM_CRITICAL \
         || level == LM_WARNING)) { \
        MainWindow::instance()->logMessage(qmsg); \
    } \
    /* Send EVERYTHING to the ACE logger (Terminal/File) */ \
    /* %T = Time, %t = Thread ID, %M = Priority Level Name */ \
    ACE_DEBUG((level, ACE_TEXT("[%T][%M][TID:%t] %s\n"), qmsg.toUtf8().constData())); \
} while (0)

namespace SimCore
{
    class Utility
    {
        public:
            static QVector3D latLonToXYZ (float m_liveOffset, float lat, float lon, float radius)
            {
                float latRad = qDegreesToRadians (lat);
                float lonRad = qDegreesToRadians (lon + m_liveOffset);

                // Matches the North Pole logic: 
                // At lat=90, sin(90)=1, so y = radius. 
                // At lat=0 (equator), sin(0)=0, so y = 0.
                float x = radius * cos (latRad) * sin (lonRad);
                float y = radius * sin (latRad);
                float z = radius * cos (latRad) * cos (lonRad);

                return QVector3D (x, y, z);
            }

            // For Satellites (Radians - faster for 20,000+ objects)
            static QVector3D latLonToXYZRad (float m_liveOffset, float latRad, float lonRad, float radius)
            {
                // Apply your -90 degree offset (converted to radians)
                float offsetRad = qDegreesToRadians (m_liveOffset);
                float adjustedLon = lonRad + offsetRad;

                return QVector3D (radius * cos (latRad) * sin (adjustedLon),
                                  radius * sin (latRad),
                                  radius * cos (latRad) * cos (adjustedLon)
                                 );
            }
    };

} // namspace Utility
