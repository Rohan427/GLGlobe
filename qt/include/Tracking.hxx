#pragma once

#ifndef TRACKING_HXX
#define TRACKING_HXX

#include "Utility.hxx"
#include "Config.hxx"

namespace Objects
{
    struct VoxelKey
    {
        int x, y, z;

        bool operator<(const VoxelKey& other) const
        {
            if (x != other.x) return x < other.x;
            if (y != other.y) return y < other.y;
            return z < other.z;
        }
    };

    class SpatialHasher
    {
        public:
            static float m_voxelSizeGL;

            static VoxelKey HashPosition (const QVector3D& pos)
            {
                // Protect against accidental divide-by-zero if uninitialized
                float size = (m_voxelSizeGL > 0.0f) ? m_voxelSizeGL : 0.0078f;

                return VoxelKey
                {
                    static_cast<int> (std::floor (pos.x() / size)),
                    static_cast<int> (std::floor (pos.y() / size)),
                    static_cast<int> (std::floor (pos.z() / size))
                };
            }
    };

    class Tracking
    {
        public:
            QVector3D m_filterAnchor;       // The position where the filter is actually running
            bool m_filterActive = false;    // Whether the shader should cull satellites
            float m_detectionRange;
            QVector3D m_satColor;
            QVector3D m_misCOlor;
            float m_RngRingDelta;

            Tracking()
            {
                // This is defined inside libsgp4 as kXKMPER (typically 6378.135)
                const double SGP4_EARTH_RADIUS = libsgp4::kXKMPER;

                m_filterAnchor = ::Config::getInstance().DEFAULT_CENTER;
                m_detectionRange = (SGP4_EARTH_RADIUS + ::Config::getInstance().DETECTION_RANGE) / SGP4_EARTH_RADIUS * Globe::globeRadius;
                m_RngRingDelta = (::Config::getInstance().RANGE_RING_DELTA / SGP4_EARTH_RADIUS) * Globe::globeRadius;
            }
    }; // class Tracking
} // namespace Objects

#endif // TRACKING_HXX
