#pragma once

#ifndef GUIDEDMISSILE_HXX
#define GUIDEDMISSILE_HXX

#include "BaseEntity.hxx"
#include "DataObjects.hxx"
#include <QVector3D>
#include <array>

enum class TargetMode
{
    AVOID_SATELLITES,
    ANTI_SATELLITE_STRIKE
};

namespace Objects
{
class GuidedMissile : public SimCore::BaseEntity
{
    Q_OBJECT

public:
    GuidedMissile (int id, size_t ssboIndex, const QVector3D& origin, const QVector3D& target);

    virtual ~GuidedMissile() = default;

    virtual void updatePhysics (float deltaTimeSec) override;

    virtual QVector3D getPosition() const override
    {
        return m_currentPos;
    }

    virtual QString getLabel() const override
    {
        return QString ("MSL-%1").arg (m_id);
    }

    // Missile-specific
    int getId() const
    {
        return m_id;
    }

    bool isActive() const
    {
        return m_active;
    }

    size_t getSsboIndex() const
    {
        return m_ssboIndex; 
    }

    void setTargetMode (TargetMode mode)
    {
        m_mode = mode;
    }

    TargetMode getTargetMode() const
    {
        return m_mode;
    }

    // === NEW: Cheap Visual Trail ===
    static constexpr int MAX_TRAIL_POINTS = 24;   // adjustable via config later

    void addTrailPoint (const QVector3D& pos);

    int getTrailCount() const
    { 
        return m_trailCount;
    }

    const QVector3D& getTrailPoint (int i) const;

private:
    int m_id;
    size_t m_ssboIndex;
    QVector3D m_currentPos;
    QVector3D m_targetPos;
    QVector3D m_velocity;
    bool m_active = true;
    TargetMode m_mode = TargetMode::ANTI_SATELLITE_STRIKE;

    // Cheap trail history (visual only)
    std::array<QVector3D, MAX_TRAIL_POINTS> m_trail;
    int m_trailHead = 0;
    int m_trailCount = 0;
    float m_trailTimer = 0.0f;        // for controlling update rate
};

} // namespace Objects

#endif // GUIDEDMISSILE_HXX
