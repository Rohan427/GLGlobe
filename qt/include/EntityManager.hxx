#pragma once

//#include "LegacyGLApp.hxx"

// SimCore/EntityManager.hxx
#include "BaseEntity.hxx"
#include <ace/Task.h>
#include <vector>

namespace SimCore
{
    class EntityManager : public ACE_Task<ACE_MT_SYNCH>
    {
        public:
            // Signal the svc() loop to terminate
            void stop()
            {
                m_done = true;
                // Also nudge the message queue in case threads are blocked on it
                this->msg_queue()->deactivate();
            }

            // ACE worker thread
            virtual int svc() override
            {
                while (!m_done && !this->msg_queue()->deactivated())
                {
                    qint64 now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

                    for (auto* entity : m_entities)
                    {
                        if (m_done) break; // Exit immediately if stopped

                        entity->updatePhysics (now, Globe::m_liveOffset);
                    }

                    ACE_OS::sleep (ACE_Time_Value (0, 50000)); // 50ms tick
                }

                return 0;
            }

            void addEntity (BaseEntity* e)
            {
                m_entities.push_back (e);
            }

            const std::vector<BaseEntity*>& getEntities() const
            {
                return m_entities;
            }

        private:
            std::vector<BaseEntity*> m_entities;
            bool m_done = false;
            qint64 m_currentTime;
    };
}
