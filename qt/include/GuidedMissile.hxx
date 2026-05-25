#pragma once

#ifndef GUIDEDMISSILE_HXX
#define GUIDEDMISSILE_HXX

#include "BaseEntity.hxx"
#include <QObject>
#include <QVector3D>
#include <QVector4D>
#include <cmath>

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
            GuidedMissile (int id, size_t ssboIndex, const QVector3D& origin, const QVector3D& target)
                            : m_id(id), m_ssboIndex (ssboIndex),
                              m_currentPos (origin),
                              m_targetPos (target),
                              m_active (true) 
            {
                m_velocity = (target - origin).normalized() * 0.05f; // Initial speed scalar
                m_mode = TargetMode::ANTI_SATELLITE_STRIKE;          // Default tactical assignment
            }

            virtual ~GuidedMissile() = default;

            // --- Polymorphic BaseEntity Overrides ---
            virtual void updatePhysics (qint64 msecs, float liveOffset) override
            {
                if (!m_active) return;

                // 1. Advance position along the linear velocity vector
                m_currentPos += m_velocity;

                // Check if the missile has reached its destination target coordinates
                if (m_currentPos.distanceToPoint (m_targetPos) < 0.01f)
                {
                    m_active = false; // Trigger destination impact termination
                }
            }

            virtual QVector3D getPosition() const override
            {
                return m_currentPos;
            }
            virtual QString getLabel() const override
            {
                return QString ("MSL-%1").arg(m_id);
            }

            // --- Missile Specific Command Interfaces ---
            int getId() const
            {
                return m_id;
            }

            bool isActive() const
            {
                return m_active;
            }

            TargetMode getTargetMode() const
            {
                return m_mode;
            }

            void setTargetMode (TargetMode mode)
            {
                m_mode = mode;
            }
            
            // Updates the 64 trailing line coordinates inside your mapped VRAM buffer
            void updateTrailGeometry(DataObjects::PathVertex* trailBufferHead);

        private:
            int m_id;
            size_t m_ssboIndex; // Unique integer mapping this weapon to a 64-vertex line block
            QVector3D m_currentPos;
            QVector3D m_targetPos;
            QVector3D m_velocity;
            bool m_active;
            TargetMode m_mode;
    };
} // namespace Objects

#endif // GUIDEDMISSILE_HXX
