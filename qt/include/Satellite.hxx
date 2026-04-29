#pragma once

#include "BaseEntity.hxx"
#include "Utility.hxx"
#include "Globe.hxx"
#include "BaseEntity.hxx"
#include <SGP4.h>
#include <Tle.h>
#include <memory>
#include <mutex>
#include <atomic>

namespace Space
{
    class Satellite : public SimCore::BaseEntity
    {
        public:
            Satellite (const QString& name, const std::string& tle1, const std::string& tle2);

            // From BaseEntity
            void updatePhysics (qint64 msecs, float liveOffset) override;
            QVector3D getPosition() const override;

            QString getLabel() const override
            {
                return m_name;
            }

            void initSatellites();

        private:
            QString m_name;
            std::unique_ptr<libsgp4::SGP4> m_propagator;
            QVector3D m_currentPos;
            mutable ACE_Thread_Mutex m_posLock;
            std::atomic<int> m_updateCount{0};
    };
} // namespace SimCore::Space
