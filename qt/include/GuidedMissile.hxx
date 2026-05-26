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
                            : m_id (id), m_ssboIndex (ssboIndex),
                              m_currentPos (origin),
                              m_targetPos (target),
                              m_active (true),
                              m_trailTimer (0.0f)
            {
                QVector3D travelDirection = (target - origin).normalized();

                // PRODUCTION REPAIR: Fetch your speed limit parameter straight from memory
                // This scales your Mach 27 step accurately across your 1.0 radius globe
                float glUnitsPerSecond = ::Config::getInstance().MAX_ICBM_SPD;

                // Apply your configured radius multiplier to protect geometry constraints
                float scaledSpeed = glUnitsPerSecond * ::Config::getInstance().DEFAULT_RADIUS;

                // Store the baseline step vector
                m_velocity = travelDirection * scaledSpeed;
            }

            virtual ~GuidedMissile() = default;

            // OVERLOAD IMPLEMENTATION: Processes clean, frame-rate independent ballistics
            virtual void updatePhysics (float deltaTimeSec) override
            {
                if (!m_active) return;

                // Advance your position cleanly relative to the actual ticking clock speed
                m_currentPos += (m_velocity * deltaTimeSec);
                
                // Stretches your trail history by accumulating fractional time increments
                m_trailTimer += deltaTimeSec;

                // Clamping threshold optimized for a 1.0 radius globe
                if (m_currentPos.distanceToPoint (m_targetPos) < 0.0005f)
                {
                    m_active = false; 
                }
            }

            virtual QVector3D getPosition() const override
            {
                return m_currentPos;
            }
            virtual QString getLabel() const override
            {
                return QString ("MSL-%1").arg (m_id);
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

            size_t getSsboIndex() const
            {
                return this->m_ssboIndex;
            }
            
            // Updates the 64 trailing line coordinates inside your mapped VRAM buffer
            void updateTrailGeometry (DataObjects::PathVertex* trailBufferHead, float deltaTimeSec);

        private:
            int m_id;
            size_t m_ssboIndex; // Unique integer mapping this weapon to a 64-vertex line block
            QVector3D m_currentPos;
            QVector3D m_targetPos;
            QVector3D m_velocity;
            bool m_active;
            TargetMode m_mode;
            float m_trailTimer; // Tracks fractional time chunks
    };
} // namespace Objects

#endif // GUIDEDMISSILE_HXX
