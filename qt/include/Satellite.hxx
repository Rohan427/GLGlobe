#pragma once

#include "BaseEntity.hxx"
#include <SGP4.h>
#include <Tle.h>
#include <memory>
#include <mutex>

namespace SimCore::Space
{
    class Satellite : public BaseEntity
    {
        public:
            Satellite (const QString& name, const std::string& tle1, const std::string& tle2);

            // From BaseEntity
            void updatePhysics (qint64 msecs) override;
            QVector3D getPosition() const override;
            QString getLabel() const override
            {
                return m_name;
            }

        private:
            QString m_name;
            std::unique_ptr<libsgp4::SGP4> m_propagator;
            QVector3D m_currentPos;
            mutable std::mutex m_posMutex; // Protect position for ACE/GL threads
    };
} // namespace SimCore::Space
#endif
