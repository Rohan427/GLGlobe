#pragma once

#include "LegacyGLApp.hxx"


// SimCore/BaseEntity.hxx
namespace SimCore
{
    class BaseEntity
    {
        public:
            virtual ~BaseEntity() = default;
            // Every object must be able to update its own 3D position
            virtual void updatePhysics(qint64 msecs) = 0;
            virtual QVector3D getPosition() const = 0;
            virtual QString getLabel() const = 0;
    };
} // namespace SimCore
