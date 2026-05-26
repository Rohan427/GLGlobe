#include "GuidedMissile.hxx"

namespace Objects
{
    void GuidedMissile::updateTrailGeometry (DataObjects::PathVertex* trailBufferHead, float deltaTimeSec)
    {
        if (!trailBufferHead || !m_active) return;

        size_t bufferOffset = m_ssboIndex * 64;

        // TARGET INTERVAL: Drop a vertex every 0.35 seconds to make the trail stretch 
        // beautifully across thousands of real-world kilometers behind the threat
        constexpr float TRAIL_DROP_INTERVAL = 0.35f; 

        if (m_trailTimer >= TRAIL_DROP_INTERVAL)
        {
            m_trailTimer = 0.0f; // Reset interval window

            // Push older positions back down the pipeline allocation array
            for (size_t i = 63; i > 0; --i)
            {
                trailBufferHead[bufferOffset + i] = trailBufferHead[bufferOffset + i - 1];
                // Smoothly fade out alpha values down the trailing edge lines
                trailBufferHead[bufferOffset + i].position.setW (static_cast<float>(63 - i) / 63.0f);
            }
        }

        // CONTINUOUS INTERPOLATION: Always lock vertex 0 (the tip) to your live coordinate
        // This stops the tail line from detaching or stuttering between interval drops
        trailBufferHead[bufferOffset].position = QVector4D (m_currentPos.x(), m_currentPos.y(), m_currentPos.z(), 1.0f);
    }
} // namespace Objects
