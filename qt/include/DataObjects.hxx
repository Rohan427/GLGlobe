#pragma once

#ifndef DATAOBJECTS_HXX
#define DATAOBJECTS_HXX

#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLBuffer>
#include <QVector3D>
#include <QVector4D>

namespace DataObjects
{
    constexpr float TYPE_DEAD_SLOT       = 0.0f; // Empty buffer array slot, skip processing
    constexpr float TYPE_SGP4_SATELLITE  = 1.0f; // Managed by CPU worker threads via SGP4
    constexpr float TYPE_KINETIC_DEBRIS  = 2.0f; // Managed by GPU Compute Shader (Falling physics)
    constexpr float TYPE_TACTICAL_MISSILE = 3.0f; // Managed by high-priority CPU/GPU path prediction
    constexpr float TYPE_INCOMING_THREAT = 4.0f; // Ballistic trajectory math

    struct PathVertex
    {
        QVector4D position; // xyz = Coordinate on the trajectory curve | w = Alpha/Fade factor
    };

    struct alignas (16) GpuEntityData
    {
        QVector4D position;  // xyz = Coordinate Vector [X,Y,Z]   | w = Entity Type Scale (1.0 = Globe)
        QVector4D velocity;  // xyz = Vector Direction [VX,VY,VZ] | w = Status Flag (1.0 = Active Sat, 0.0 = Debris)
        QVector4D metadata;  // x = Lifespan Decay Timer          | y = Mass Parameter   | z/w = Reserved Tactical Flags
    };
} // namespace DataObjects

#endif // DATAOBJECTS_HXX
