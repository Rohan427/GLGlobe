#include "Satellite.hxx"

namespace Space 
{
    using namespace SimCore;

    Satellite::Satellite (const QString& name, const std::string& tle1, const std::string& tle2) 
        : m_name (name) 
    {
        libsgp4::Tle tle (name.toStdString(), tle1, tle2);
        m_propagator = std::make_unique<libsgp4::SGP4> (tle);
    }

    void Satellite::updatePhysics (qint64 msecs, float liveOffset)
    {
        if (!m_propagator) return;

        // Convert msecs to SGP4 DateTime
        QDateTime qtTime = QDateTime::fromMSecsSinceEpoch (msecs, Qt::UTC);
        libsgp4::DateTime dt (qtTime.date().year(), qtTime.date().month(), qtTime.date().day(),
                              qtTime.time().hour(), qtTime.time().minute(), qtTime.time().second());

        try
        {
            libsgp4::Eci eci = m_propagator->FindPosition(dt);
            libsgp4::CoordGeodetic geo = eci.ToGeodetic();

            float altMultiplier = (6371.0f + (float)geo.altitude) / 6371.0f;
            QVector3D newPos = Utility::latLonToXYZ (liveOffset, geo.latitude, geo.longitude, Globe::globeRadius * altMultiplier);

            ACE_GUARD (ACE_Thread_Mutex, ace_mon, m_posLock);
            m_currentPos = newPos;
        } 
        catch (...)
        {
            // Handle decay or math errors silently for the worker thread
        }
    }

    QVector3D Satellite::getPosition() const
    {
        ACE_GUARD_RETURN (ACE_Thread_Mutex, ace_mon, m_posLock, QVector3D());
        return m_currentPos;
    }
} // namespace Space
