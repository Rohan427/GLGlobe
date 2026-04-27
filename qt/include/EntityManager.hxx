#pragma once

#include "LegacyGlApp"

// SimCore/EntityManager.hxx
#include <ace/Task.h>
#include <vector>

namespace SimCore
{
    class EntityManager : public ACE_Task<ACE_MT_SYNCH>
    {
        public:
            // ACE worker thread
            virtual int svc() override
            {
                while (!done_)
                {
                    for (auto* entity : m_entities)
                    {
                        entity->updatePhysics (m_currentTime);
                    }

                    ACE_OS::sleep (ACE_Time_Value (0, 50000)); // 50ms tick
                }

                return 0;
            }

            void addEntity(BaseEntity* e)
            {
                m_entities.push_back(e);
            }

            const std::vector<BaseEntity*>& getEntities() const
            {
                return m_entities;
            }

        private:
            std::vector<BaseEntity*> m_entities;
            bool done_ = false;
            qint64 m_currentTime;
    };
}
