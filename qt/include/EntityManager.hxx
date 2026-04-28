#pragma once

// SimCore/EntityManager.hxx
#include "BaseEntity.hxx"
#include <ace/Task.h>
#include <vector>
#include <atomic>
//#include "MainWindow.hxx"

namespace SimCore
{
    class EntityManager : public ACE_Task<ACE_MT_SYNCH>
    {
        public:
            // Thread-safe way for GUI or Network thread to add entities
            void addEntity (BaseEntity* e)
            {
 //               MainWindow::instance()->logMessage ("Adding Entity");
                ACE_GUARD (ACE_Thread_Mutex, mon, m_vectorLock);
                m_entities.push_back (e);
            }

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
                // Get the unique ID of this thread within the task pool (0 to 31)
                int threadId = this->grp_id();
                // Assign a unique 0-31 index to this specific thread instance
                int localThreadId = m_threadIndexer.fetch_add(1); 
                int totalThreads = 32;

                while (!m_done && !this->msg_queue()->deactivated())
                {
                    qint64 now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
                    {
                        // Striped Partitioning: Each thread handles every Nth satellite
                        // We lock briefly to ensure the vector doesn't resize during our loop
                        ACE_GUARD_RETURN (ACE_Thread_Mutex, mon, m_vectorLock, -1);

                        for (size_t i = (size_t)localThreadId; i < m_entities.size(); i += totalThreads)
                        {
                            if (m_done) break; // Exit immediately if stopped
                            m_entities[i]->updatePhysics (now, Globe::m_liveOffset);
                        }
                    }

                    ACE_OS::sleep (ACE_Time_Value (0, Globe::THREAD_SLEEP_TIME));
                }

                return 0;
            }

            const std::vector<BaseEntity*>& getEntities() const
            {
                return m_entities;
            }

            void onDataReceived (const QString& data);

            // Static helper for the parsing thread
            static void* parsingTask (void* arg)
            {
                QString* rawData = static_cast<QString*> (arg);
                
                // 1. Heavy String Splitting
                QStringList lines = rawData->split ('\n', Qt::SkipEmptyParts);
                
                // 2. Create objects and add to manager
                // Note: You'll need an ACE_Thread_Mutex around m_entities.push_back()
                
                delete rawData;

                return nullptr;
            }

            void processTleData (const QString& data);

        private:
            std::vector<BaseEntity*> m_entities;
            bool m_done = false;
            qint64 m_currentTime;
            ACE_Thread_Mutex m_vectorLock; // Protects the vector itself
            std::atomic<int> m_threadIndexer{0};
    };
}
