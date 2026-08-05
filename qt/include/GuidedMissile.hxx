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
            mutable ACE_Thread_Mutex m_trailLock;

            GuidedMissile (int id, size_t ssboIndex, const QVector3D& origin, const QVector3D& target);

            virtual ~GuidedMissile() = default;
            virtual void updatePhysics (DataObjects::GpuEntityData missileData, float deltaTimeSec, bool detected) override;
            void updateVelocity (QVector4D velVector);
            bool isInsideSensorVolume (const QVector3D& currentPos,
                                       bool filterEnabled,
                                       const QVector3D& filterCenter,
                                       float filterRadius
                                      );

            const QVector3D& getTrailPoint (int i) const;
            void deactivate();
            void addTrailPoint (const QVector3D& pos);
            void updateMissileFromGPU (DataObjects::GpuEntityData missileData);

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

            size_t getTrailCount() const
            { 
                return m_trailCount;
            }

        private:
            int m_id;
            size_t m_ssboIndex;
            QVector3D m_currentPos;
            QVector3D m_targetPos;
            QVector3D m_velocity;
            bool m_active = true;
            TargetMode m_mode = TargetMode::ANTI_SATELLITE_STRIKE;
            bool m_detected = false;

            // Cheap trail history (visual only)
            std::vector<QVector3D> m_trail{};
            size_t m_trailHead = 0;
            size_t m_trailCount = 0;
            float m_trailTimer = 0.0f;        // for controlling update rate
    };
} // namespace Objects

#endif // GUIDEDMISSILE_HXX
