#pragma once

#include <QtMath>
#include <QVector3D>
#include <QString>
#include <QDateTime>


#define SIM_LOG(msg) \
    do { \
    QString qmsg = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz")).arg(msg); \
        if (MainWindow::instance()) { \
        MainWindow::instance()->logMessage(qmsg); \
    } \
    std::clog << qmsg.toStdString() << std::endl; \
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
