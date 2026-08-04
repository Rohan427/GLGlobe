#include "GuidedMissile.hxx"
#include "Config.hxx"

namespace Objects
{
    GuidedMissile::GuidedMissile (int id, size_t ssboIndex, const QVector3D& origin, const QVector3D& target)
        : m_id (id)
        , m_ssboIndex (ssboIndex)
        , m_currentPos (origin)
        , m_targetPos (target)
    {
        QVector3D dir = (target - origin).normalized();
        float speed = ::Config::getInstance().MAX_ICBM_SPD * ::Config::getInstance().DEFAULT_RADIUS;
        m_velocity = dir * speed;
    }

    void GuidedMissile::updateVelocity (QVector4D velVector)
    {
        QVector3D dir (velVector.x(), velVector.y(), velVector.z());
        float speed = velVector.w();                 // already * glScaleFactor
        m_velocity = dir.normalized() * speed;
    }

    void GuidedMissile::updatePhysics (DataObjects::GpuEntityData missileData, float deltaTimeSec, bool detected)
    {
        if (missileData.metadata.w() == 0.0f)
        {
            deactivate();
            return;
        }

        if (!m_active)
        {
            return;
        }

        QVector3D pos (missileData.position.x(), missileData.position.y(), missileData.position.z());
        updateVelocity (missileData.velocity);

        m_currentPos += m_velocity * deltaTimeSec;
        m_trailTimer += deltaTimeSec;

        // Update trail at a controlled rate (visual only)
        if (m_trailTimer >= ::Config::getInstance().SENS_UPDATE_RATE)   // ~30 Hz instead of 12 Hz
        {
            if (detected)
            {
                addTrailPoint (pos);
            }

            m_trailTimer = 0.0f;
        }

        //std::cout << "Missile " << m_id << " Target position: (" << m_targetPos.x() << ", "
        //                                                         << m_targetPos.y() << ", "
        //                                                         << m_targetPos.z() << ")" << std::endl;

        //std::cout << "Missile " << m_id << " Missile position: (" << m_currentPos.x() << ", "
        //                                                          << m_currentPos.y() << ", "
        //                                                          << m_currentPos.z() << ")" << std::endl;

        // Impact detection
        if (m_currentPos.distanceToPoint (m_targetPos) < 0.01f)
        {
            deactivate();
        }
    }

    bool GuidedMissile::isInsideSensorVolume (const QVector3D& currentPos,
                                              bool filterEnabled,
                                              const QVector3D& filterCenter,
                                              float filterRadius
                                             ) const
    {
        if (!filterEnabled)
        {
            return false;   // or false if “no filter → no trails”
        }

        // SAFETY: near origin / core
        if (currentPos.length() < 0.01f)
        {
            return false;
        }

        // A. Horizontal max sensor range
        if (QVector3D::dotProduct (currentPos - filterCenter, currentPos - filterCenter) > filterRadius * filterRadius)
        {
            return false;
        // (or currentPos.distanceToPoint(filterCenter) > filterRadius)
        }

        // B. Horizon plane: object must be on the outward side of the tangent plane at filterCenter
        const QVector3D toObject = currentPos - filterCenter;
        const QVector3D planeNormal = filterCenter.normalized();  // “up” at sensor

        if (QVector3D::dotProduct (toObject, planeNormal) < -0.0005f)
        {
            return false;
        }

        return true;
    }

    void GuidedMissile::addTrailPoint (const QVector3D& pos)
    {
        ACE_GUARD (ACE_Thread_Mutex, mon, m_trailLock);

        if (!m_active)
        {
            return;
        }

        m_trail[m_trailHead] = pos;
        m_trailHead = (m_trailHead + 1) % ::Config::getInstance().MAX_MISSILE_POINTS;

        if (m_trailCount < ::Config::getInstance().MAX_MISSILE_POINTS)
        {
            m_trailCount++;
        }
    }

    const QVector3D& GuidedMissile::getTrailPoint (int i) const
    {
        ACE_GUARD_RETURN (ACE_Thread_Mutex, mon, m_trailLock, m_currentPos);

        if (m_trailCount == 0 || i < 0 || i >= static_cast<int> (m_trailCount))
        {
            return m_currentPos;   // fallback
        }

        return m_trail[i];
    }

    void GuidedMissile::deactivate()
    {
        ACE_GUARD (ACE_Thread_Mutex, mon, m_trailLock);

        m_active = false;
        m_trailCount = 0;
        m_trailHead = 0;
    }
} // namespace Objects
