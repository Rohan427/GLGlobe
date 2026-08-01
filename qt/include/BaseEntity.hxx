#pragma once

#ifndef BASEENTITY_HXX
#define BASEENTITY_HXX

#ifndef ACE_MT_SAFE
#define ACE_MT_SAFE 1
#endif

#include "Utility.hxx"
#include "DataObjects.hxx"
#include <QObject>
#include <QVector3D>
#include <QString>
#include "ObjectPool.hxx"

namespace SimCore
{
    class BaseEntity : public QObject
    {
        Q_OBJECT

        public:
            virtual ~BaseEntity() = default;

            // =====================================================================
            // Physics Updates
            // =====================================================================
            virtual void updatePhysics (qint64 msecs, float liveOffset) {}     // Satellites (SGP4)
            virtual void updatePhysics (DataObjects::GpuEntityData missileData, float deltaTimeSec) {}                 // Missiles / tactical

            // =====================================================================
            // Required Getters
            // =====================================================================
            virtual QVector3D getPosition() const = 0;
            virtual QString   getLabel() const = 0;

            // =====================================================================
            // GPU Metadata Getters (default safe implementations)
            // =====================================================================
            virtual float     getLifespan() const          { return 999999.0f; }   // Satellites live "forever"
            virtual float     getThrust() const            { return 0.0f; }
            virtual float     getSpeed() const             { return 0.0f; }
            virtual float     getMass() const              { return 1.0f; }
            virtual float     getStateId() const           { return DataObjects::STATE_BALLISTIC; } // or 10.0f

            virtual QVector3D getVelocityDirection() const { return QVector3D (0.0f, 0.0f, 1.0f); }

            // Optional: can be used for filtering / labels
            virtual QString   getGroup() const             { return QString(); }
            virtual QString   getNoradId() const           { return QString(); }
    };
} // namespace SimCore

#endif // BASEENTITY_HXX
