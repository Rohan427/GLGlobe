#include "Satellite.hxx"
#include "MainWindow.hxx"
#include "DataObjects.hxx"

namespace Space
{
    using namespace SimCore;

    ACE_Thread_Mutex Satellite::lock_;
    int Satellite::tleErrors = 0;

    Satellite::Satellite (const QString& name, const std::string& tle1,
                         const std::string& tle2, const QString& group)
        : m_name(name), m_group (group)
    {
        try
        {
            libsgp4::Tle tle (name.toStdString(), tle1, tle2);
            m_propagator = std::make_unique<libsgp4::SGP4> (tle);
            m_noradId = QString::fromStdString (tle1.substr(2, 5));
        }
        catch (...)
        {
            ++tleErrors;
        }
    }

    Satellite::~Satellite() = default;

    void Satellite::updatePhysics (qint64 msecs, float liveOffset)
    {
        if (!m_propagator)
        {
            return;
        }

        QDateTime qtTime = QDateTime::fromMSecsSinceEpoch (msecs, Qt::UTC);
        int microsecs = qtTime.time().msec() * 1000;

        libsgp4::DateTime dt (qtTime.date().year(), qtTime.date().month(), qtTime.date().day(),
                              qtTime.time().hour(), qtTime.time().minute(), qtTime.time().second(),
                              microsecs
                             );

        try
        {
            libsgp4::Eci eci = m_propagator->FindPosition (dt);
            libsgp4::CoordGeodetic geo = eci.ToGeodetic();

            const double SGP4_EARTH_RADIUS = libsgp4::kXKMPER;
            double scaleFactor = Globe::globeRadius / SGP4_EARTH_RADIUS;
            float visualRadius = Globe::globeRadius + (static_cast<float> (geo.altitude) * static_cast<float> (scaleFactor));

            QVector3D newPos = Utility::latLonToXYZRad (liveOffset, geo.latitude, geo.longitude, visualRadius);

            ACE_GUARD (ACE_Thread_Mutex, ace_mon, m_posLock);
            m_currentPos = newPos;
        }
        catch (...)
        {
            // Silent fail - position stays at last known good
        }
    }

    QVector3D Satellite::getPosition() const
    {
        ACE_GUARD_RETURN (ACE_Thread_Mutex, ace_mon, m_posLock, QVector3D());
        return m_currentPos;
    }

    QVector3D Satellite::getVelocityDirection() const
    {
        if (!m_propagator)
        {
            return QVector3D(0.0f, 0.0f, 1.0f);
        }

        try
        {
            // Use current time (you can cache this if performance becomes an issue)
            auto now = QDateTime::currentDateTimeUtc();
            int microsecs = now.time().msec() * 1000;

            libsgp4::DateTime dt (now.date().year(), now.date().month(), now.date().day(),
                                  now.time().hour(), now.time().minute(), now.time().second(), microsecs
                                 );

            libsgp4::Eci eci = m_propagator->FindPosition (dt);
            libsgp4::Vector vel = eci.Velocity();        // libsgp4 provides this

            QVector3D direction (vel.x, vel.y, vel.z);
            float len = direction.length();
            return (len > 0.001f) ? direction.normalized() : QVector3D (0.0f, 0.0f, 1.0f);
        }
        catch (...)
        {
            return QVector3D(0.0f, 0.0f, 1.0f);
        }
    }

    void Satellite::initSatellites()
    {
        // Test / demo satellite (ISS) if needed
    }

    void Satellite::resetTleErrors()
    {
        ACE_GUARD (ACE_Thread_Mutex, ace_mon, lock_);
        tleErrors = 0;
    }

} // namespace Space
