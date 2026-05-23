// SpacialHasher.hxx
#pragma once
#include <QVector3D>
#include <cmath>

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
        // DYNAMIC REPAIR: Swap compile-time constexpr out for a runtime static handle
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


#include "SpatialHasher.hxx"

// Define the static storage space in memory
float SpatialHasher::m_voxelSizeGL = 0.0078f; 



void MyGLWidget::initializeSimulationMetrics()
{
    double earthRadiusKm = libsgp4::kXKMPER; // Exact SGP4 reference model base
    float configuredRadius = ::Config::getInstance().DEFAULT_RADIUS;

    // YOUR VERIFIED EQUATION: Compute the 50km world space bounding size dynamically
    float targetCoverageKm = 50.0f;
    SpatialHasher::m_voxelSizeGL = (configuredRadius / static_cast<float>(earthRadiusKm)) * targetCoverageKm;

    qDebug() << "Spatial Hashing Configuration Realignment Complete.";
    qDebug() << "Calculated Voxel Unit Target Boundary Size:" << SpatialHasher::m_voxelSizeGL << "GL units (~50km)";
}



// Inside your EntityManager processing matrix loop or dedicated tracking class
#include <map>
#include <vector>

void EntityManager::processCollisionsAndIntersects (qint64 currentMsecs, int localThreadId)
{
    // 1. CLEAR THREAD-LOCAL SPATIAL CACHE
    // Each thread manages its own isolated bucket to guarantee zero cross-talk or lock stalls
    std::map<VoxelKey, std::vector<BaseEntity*>> localSpatialGrid;
    
    size_t entityCount = m_entities.size();

    // 2. PHASE 1: BATCH SATELLITES INTO GEOMETRIC BUCKETS
    // Run your safe interleaved stride matching your active thread allocation count
    for (size_t i = static_cast<size_t> (localThreadId); i isSatellite())
    {
                // Fetch the satellite's newly updated world position vector [X, Y, Z]
                QVector3D satPos = entity->getWorldPosition(); 
                VoxelKey key = SpatialHasher::HashPosition (satPos);
                
                localSpatialGrid[key].push_back (entity);
            }
        }
    }

    // 3. PHASE 2: INTERSECT MISSILES AGAINST LOCAL BUCKETS
    // Let your missile/trajectory list be parsed across your thread architecture
    size_t missileCount = m_missiles.size();
    
    // Scale factor for true geometric contact (e.g., proximity fuze range of 5 kilometers)
    float collisionThresholdGL = 0.0012f; 

    for (size_t m = static_cast<size_t> (localThreadId); m < missileCount; m += m_numThreads)
    {
        if (m < m_missiles.size())
        {
            GuidedMissile* missile = m_missiles[m];

            if (!missile || !missile->isActive())
            {
                continue;
            }

            QVector3D missilePos = missile->getWorldPosition();
            VoxelKey missileKey  = SpatialHasher::HashPosition (missilePos);

            // OPTIMIZATION: Check only the voxel the missile is inside, plus adjacent cubes
            for (int dx = -1; dx <= 1; ++dx)
            {
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dz = -1; dz <= 1; ++dz)
                    {
                        
                        VoxelKey neighborKey {missileKey.x + dx, missileKey.y + dy, missileKey.z + dz};
                        auto it = localSpatialGrid.find (neighborKey);
                        
                        if (it != localSpatialGrid.end())
                        {
                            // Loop only through the tiny handful of satellites in this immediate 3D block
                            for (BaseEntity* satellite : it->second)
                            {
                                float distance = missilePos.distanceToPoint (satellite->getWorldPosition());
                                
                                if (distance <= collisionThresholdGL)
                                {
                                    // =========================================================
                                    // TACTICAL ROUTING INTERCEPT
                                    // =========================================================
                                    if (missile->getTargetMode() == TargetMode::AVOID_SATELLITES)
                                    {
                                        // Trigger evasive vector calculation redirection loop
                                        missile->recalculateEvasivePath (satellite->getWorldPosition());
                                    } 
                                    else if (missile->getTargetMode() == TargetMode::ANTI_SATELLITE_STRIKE)
                                    {
                                        // Detonate proximity warhead, mark objects for destruction
                                        satellite->flagAsDestroyed();
                                        missile->detonate();
                                        
                                        SIM_LOG (LM_INFO, QString ("TACTICAL INTERCEPT: Missile %1 destroyed Satellite %2 over range ring coordinates!")
                                                 .arg(missile->getId())
                                                 .arg (satellite->getId())
                                                );
                                    }
                                }
                            }
                        }
                    }
                }
            } // End neighborhood voxel search loop
        }
    }
}

/*
Evasive Maneuvering Input: For missiles flagged to avoid satellites, the recalculateEvasivePath() function can add a
simple perpendicular push vector based on the cross product of the missile's current velocity and the direction vector
to the nearby satellite:
*/

QVector3D avoidDir = QVector3D::crossProduct(missileVelocity, satelliteDirection).normalized();
missileVelocity += avoidDir * avoidanceStrength;


// View (The Camera/Mouse controls - Dynamic Scale Multiplier Repair)
QMatrix4x4 view;
float radiusScalar = ::Config::getInstance().DEFAULT_RADIUS * 6.66667f; // Automatically converts 1.0 back to a proportional -6.66f limit

view.translate(Globe::m_offset.x(), Globe::m_offset.y(), -radiusScalar * Globe::m_zoom);
view.rotate(Globe::m_rotation.x(), 1.0f, 0.0f, 0.0f);
view.rotate(Globe::m_rotation.y(), 0.0f, 1.0f, 0.0f);



        while (!m_done)
        {
            auto now = std::chrono::high_resolution_clock::now();
            auto duration = now.time_since_epoch();
            qint64 msecs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

            if (m_vectorLock.tryacquire() == 0)
            {
                size_t currentSize = m_entities.size();

                if (currentSize > 0)
                {
                    // Thread-localized grid allocation ensures complete thread safety with NO mutexes
                    std::map<VoxelKey, std::vector<BaseEntity*>> localSpatialGrid;

                    // PHASE 1: BATCH SATELLITES INTO LOCAL GEOMETRIC BUCKETS
                    // Use your verified m_numThreads stride to process your interleaved slice safely
                    for (size_t i = static_cast<size_t>(localThreadId); i isSatellite())
                    {
                        entity->updatePhysics (msecs, Globe::m_liveOffset);
                        
                        // Map the satellite to its new 1.0 scale voxel grid slot
                        VoxelKey key = SpatialHasher::HashPosition (entity->getWorldPosition());
                        localSpatialGrid[key].push_back (entity);
                    }
                }
            }

            // PHASE 2: EVALUATE MISSILE INTERCEPT AND AVOIDANCE PATHS
            // (Your thread-localized voxel search loop runs here against localSpatialGrid)
            // ...
        }

        m_vectorLock.release();
    }
    else
    {
        ACE_Thread::yield();
    }

    // Handle loop rate pacing based on sleep configuration definitions
    if (::Config::getInstance().THREAD_SLEEP_TIME > 0)
    {
        ACE_OS::sleep(ACE_Time_Value(0, ::Config::getInstance().THREAD_SLEEP_TIME));
    }
    else
    {
        ACE_OS::sleep(ACE_Time_Value(0, ::Config::getInstance().DEFAULT_THREAD_SLEEP));
    }
}
