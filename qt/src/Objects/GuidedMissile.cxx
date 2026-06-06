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

        // Seed the trail with starting position
        addTrailPoint (origin);
    }

    void GuidedMissile::updatePhysics (float deltaTimeSec)
    {
        if (!m_active) 
        {
            return;
        }

        m_currentPos += m_velocity * deltaTimeSec;
        m_trailTimer += deltaTimeSec;

        // Update trail at a controlled rate (visual only)
        if (m_trailTimer >= 0.033f)   // ~30 Hz instead of 12 Hz
        {
            addTrailPoint(m_currentPos);
            m_trailTimer = 0.0f;
        }

        // Impact detection
        if (m_currentPos.distanceToPoint (m_targetPos) < 0.001f)
        {
            m_active = false;
        }
    }

    void GuidedMissile::addTrailPoint (const QVector3D& pos)
    {
        m_trail[m_trailHead] = pos;
        m_trailHead = (m_trailHead + 1) % MAX_TRAIL_POINTS;

        if (m_trailCount < MAX_TRAIL_POINTS)
        {
            m_trailCount++;
        }
    }

    const QVector3D& GuidedMissile::getTrailPoint (int i) const
    {
        // Safe wrap-around access
        return m_trail[i % MAX_TRAIL_POINTS];
    }

} // namespace Objects
