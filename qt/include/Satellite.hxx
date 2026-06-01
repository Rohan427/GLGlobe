#pragma once

#ifndef SATELLITE_HXX
#define SATELLITE_HXX

#include "BaseEntity.hxx"
#include "Utility.hxx"
#include "Globe.hxx"
#include <SGP4.h>
#include <Tle.h>
#include <memory>
#include <atomic>

namespace Space
{
    class Satellite : public SimCore::BaseEntity
    {
        public:
            Satellite(const QString& name, const std::string& tle1,
                      const std::string& tle2, const QString& group);

            ~Satellite() override;

            // BaseEntity overrides
            void updatePhysics(qint64 msecs, float liveOffset) override;
            QVector3D getPosition() const override;
            QString getLabel() const override { return m_name; }

            QString getGroup() const override { return m_group; }
            QString getNoradId() const override { return m_noradId; }

            // GPU metadata overrides
            float getLifespan() const override { return 999999.0f; }   // Satellites don't expire
            float getStateId() const override  { return DataObjects::STATE_BALLISTIC; }

            // Real orbital velocity direction from SGP4
            QVector3D getVelocityDirection() const override;

            void initSatellites();

            static int getTleErrors() { return tleErrors; }
            static void resetTleErrors();

            static ACE_Thread_Mutex lock_;

        private:
            QString m_name;
            QString m_noradId;
            std::unique_ptr<libsgp4::SGP4> m_propagator;
            QVector3D m_currentPos;
            mutable ACE_Thread_Mutex m_posLock;
            std::atomic<int> m_updateCount{0};
            QString m_group;

            static int tleErrors;
    };
} // namespace Space

#endif // SATELLITE_HXX
