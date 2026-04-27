#pragma once

#include <QtMath>

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
    };

} // namspace Utility
