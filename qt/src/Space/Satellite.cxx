#include "Satellite.hxx"
#include <QtMath>

namespace SimCore::Space {

Satellite::Satellite(const QString& name, const std::string& tle1, const std::string& tle2) 
    : m_name(name) 
{
    libsgp4::Tle tle(name.toStdString(), tle1, tle2);
    m_propagator = std::make_unique<libsgp4::SGP4>(tle);
}

void Satellite::updatePhysics(qint64 msecs) {
    if (!m_propagator) return;

    // Convert msecs to SGP4 DateTime
    QDateTime qtTime = QDateTime::fromMSecsSinceEpoch(msecs, Qt::UTC);
    libsgp4::DateTime dt(qtTime.date().year(), qtTime.date().month(), qtTime.date().day(),
                         qtTime.time().hour(), qtTime.time().minute(), qtTime.time().second());

    try {
        libsgp4::Eci eci = m_propagator->FindPosition(dt);
        libsgp4::CoordGeodetic geo = eci.ToGeodetic();

        // Map to 3D Sphere (using your established radius scaling)
        float lat = qRadiansToDegrees(geo.latitude);
        float lon = qRadiansToDegrees(geo.longitude);
        float drawRadius = 1.5f * ((6371.0f + (float)geo.altitude) / 6371.0f);

        // Your proven XYZ math
        float latRad = qDegreesToRadians(lat);
        float lonRad = qDegreesToRadians(lon); // Apply your -90 sync if needed here
        
        QVector3D newPos(
            drawRadius * cos(latRad) * sin(lonRad),
            drawRadius * sin(latRad),
            drawRadius * cos(latRad) * cos(lonRad)
        );

        std::lock_guard<std::mutex> lock(m_posMutex);
        m_currentPos = newPos;
    } catch (...) {
        // Handle decay or math errors silently for the worker thread
    }
}

QVector3D Satellite::getPosition() const {
    std::lock_guard<std::mutex> lock(m_posMutex);
    return m_currentPos;
}

} // namespace SimCore::Space
