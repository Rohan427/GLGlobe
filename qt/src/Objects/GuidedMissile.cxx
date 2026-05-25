#include "GuidedMissile.hxx"

namespace Objects
{
    void GuidedMissile::updateTrailGeometry (DataObjects::PathVertex* trailBufferHead)
    {
        if (!trailBufferHead || !m_active) return;

        // Compute starting vertex memory slot offset inside your global array allocation
        size_t bufferOffset = m_ssboIndex * 64;

        // Simple real-time history shift: pushes older coordinates down the line
        for (size_t i = 63; i > 0; --i) {
            trailBufferHead[bufferOffset + i] = trailBufferHead[bufferOffset + i - 1];
            // Slowly reduce the alpha parameter to fade older segments out over time
            trailBufferHead[bufferOffset + i].position.setW(static_cast<float>(63 - i) / 63.0f);
        }

        // Drop the freshly calculated current position straight into the head node
        trailBufferHead[bufferOffset].position = QVector4D(m_currentPos.x(), m_currentPos.y(), m_currentPos.z(), 1.0f);
    }
} // namespace Objects
