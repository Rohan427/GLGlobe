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
        if (!m_propagator)
        {
            std::cout << "No propagator" << std::endl;

            return;
        }

        // Convert msecs to SGP4 DateTime
        QDateTime qtTime = QDateTime::fromMSecsSinceEpoch (msecs, Qt::UTC);

        libsgp4::DateTime dt (qtTime.date().year(), qtTime.date().month(), qtTime.date().day(),
                              qtTime.time().hour(), qtTime.time().minute(), qtTime.time().second());

        try
        {
            libsgp4::Eci eci = m_propagator->FindPosition (dt);
            libsgp4::CoordGeodetic geo = eci.ToGeodetic();

            float altMultiplier = (6371.0f + (float)geo.altitude) / 6371.0f;
            QVector3D newPos = Utility::latLonToXYZ (liveOffset, geo.latitude, geo.longitude, Globe::globeRadius * altMultiplier);

            ACE_GUARD (ACE_Thread_Mutex, ace_mon, m_posLock);
            m_currentPos = newPos;
        } 
        catch (...)
        {
//            SimCore::MainWindow::instance()->logMessage (QString ("SGP4 Error: %1").arg (e.what()));
            std::cout << "TLE Exception: " << std::endl;
        }
    }

    QVector3D Satellite::getPosition() const
    {
//        std::cout << "Satellite: x, y, z: " << m_currentPos.x() << ", " << m_currentPos.y() << ", " << m_currentPos.z() << std::endl; 
        ACE_GUARD_RETURN (ACE_Thread_Mutex, ace_mon, m_posLock, QVector3D());
        return m_currentPos;
    }

    void Satellite::initSatellites()
    {
        // Use Raw String Literals R"(...)" to ensure no escape-character issues
        std::string l1 = R"(1 25544U 98067A   26116.51782528  .00002182  00000-0  10000-3 0  9993)";
        std::string l2 = R"(2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537)";
/*
        try
        {
            // 2. Create the Tle object
            libsgp4::Tle tle ("ISS", l1, l2);

            // 3. Instantiate the SGP4 propagator into your unique_ptr
            // This is where m_issPropagator finally stops being null
            m_Propagator = std::make_unique<libsgp4::SGP4> (tle);

            MainWindow::instance()->logMessage ("ISS Propagator initialized successfully.");
        }
        catch (const std::exception& e)
        {
            MainWindow::instance()->logMessage (QString ("SGP4 Error: %1").arg (e.what()));
        }
*/
    }
} // namespace Space
