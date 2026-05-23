#pragma once

#ifndef SATELLITE_HXX
#define SATELLITE_HXX

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
            // Update constructor to accept the group key
            Satellite (const QString& name, const std::string& tle1, const std::string& tle2, const QString& group);

            ~Satellite()
            {
                m_propagator.reset();
            }

            
            QString getGroup() const { return m_group; }

            // From BaseEntity
            void updatePhysics (qint64 msecs, float liveOffset) override;
            QVector3D getPosition() const override;

            QString getLabel() const override
            {
                return m_name;
            }

            void initSatellites();

            static int getTleErrors()
            {
                return tleErrors;
            }

            static void resetTleErrors()
            {
                ACE_GUARD(ACE_Thread_Mutex, ace_mon, lock_);
                tleErrors = 0;
            }
             
            QString getNoradId() const
            {
                return m_noradId;
            }
            
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
} // namespace SimCore::Space

#endif // SATELLITE_HXX
