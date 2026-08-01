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
//        SIM_LOG (LM_INFO, "GuidedMissile::GuidedMissile()");
        QVector3D dir = (target - origin).normalized();
        float speed = ::Config::getInstance().MAX_ICBM_SPD * ::Config::getInstance().DEFAULT_RADIUS;
        m_velocity = dir * speed;

        // Seed the trail with starting position
        addTrailPoint (origin);
    }

    void GuidedMissile::updateVelocity (QVector4D velVector)
    {
        QVector3D dir (velVector.x(), velVector.y(), velVector.z());
        float speed = velVector.w();                 // already * glScaleFactor
        m_velocity = dir.normalized() * speed;
    }

    void GuidedMissile::updatePhysics (DataObjects::GpuEntityData missileData, float deltaTimeSec)
    {
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
            addTrailPoint (pos);
            m_trailTimer = 0.0f;
        }

        // Impact detection
        if (m_currentPos.distanceToPoint (m_targetPos) < 0.001f)
        {
            deactivate();
        }
    }

    void GuidedMissile::addTrailPoint (const QVector3D& pos)
    {
        ACE_GUARD (ACE_Thread_Mutex, mon, m_trailLock);

        if (!m_active)
        {
            return;
        }

        //SIM_LOG (LM_INFO, QString ("addTrailPoint m_id %1: (%2, %3, %4)").arg (m_id)
        //         .arg (pos.x()).arg (pos.y()).arg (pos.z()));

        m_trail[m_trailHead] = pos;
        m_trailHead = (m_trailHead + 1) % ::Config::getInstance().MAX_MISSILE_POINTS;

        if (m_trailCount < ::Config::getInstance().MAX_MISSILE_POINTS)
        {
            m_trailCount++;
        }

//        SIM_LOG (LM_INFO, QString ("Trail Count %1").arg (m_trailCount));
    }

    const QVector3D& GuidedMissile::getTrailPoint (int i) const
    {
//        SIM_LOG (LM_INFO, QString ("GuidedMissile::getTrailPoint(), m_id %1").arg (m_id));
        ACE_GUARD_RETURN (ACE_Thread_Mutex, mon, m_trailLock, m_currentPos);

        if (m_trailCount == 0|| i < 0 || i >= static_cast<int>(m_trailCount))
        {
//            SIM_LOG (LM_INFO, QString ("GuidedMissile::getTrailPoint() return m_currentPos, m_id %1").arg (m_id));
            return m_currentPos;   // fallback
        }

        // Correct circular buffer indexing: oldest to newest
        int realIndex = (m_trailHead - m_trailCount + i + MAX_TRAIL_POINTS) % MAX_TRAIL_POINTS;

//        SIM_LOG (LM_INFO, QString ("GuidedMissile::getTrailPoint() return m_trail[%1], m_id %2").arg (realIndex).arg (m_id));
        return m_trail[realIndex];
    }

    void GuidedMissile::deactivate()
    {
//        SIM_LOG (LM_INFO, QString ("GuidedMissile::deactivate() m_id %1").arg (m_id));
        ACE_GUARD (ACE_Thread_Mutex, mon, m_trailLock);

        m_active = false;
        m_trailCount = 0;
        m_trailHead = 0;
        // array is automatically "cleared" by count = 0
    }
} // namespace Objects
