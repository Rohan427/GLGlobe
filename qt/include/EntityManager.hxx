#pragma once

// SimCore/EntityManager.hxx
#include "BaseEntity.hxx"
#include <vector>
#include <atomic>
#include <algorithm>

namespace SimCore
{
    class EntityManager : public QObject, public ACE_Task<ACE_MT_SYNCH>
    {        
        Q_OBJECT

        private:
            static EntityManager* s_instance;
            std::vector<BaseEntity*> m_entities;
            bool m_done = false;
            qint64 m_currentTime;
            static ACE_Thread_Mutex m_vectorLock; // Protects the vector itself
            std::atomic<int> m_threadIndexer {0};
            ACE_Barrier* m_barrier = nullptr;
            int m_numThreads;

            std::unordered_set<QString> m_activeIds; // Fast lookup for duplicates

        public:
            static EntityManager* instance();

            void startSimulation (int numThreads);

            ~EntityManager()
            {
                delete m_barrier;
            }

            // Thread-safe way for GUI or Network thread to add entities
            void addEntity (BaseEntity* e)
            {
                ACE_GUARD (ACE_Thread_Mutex, mon, m_vectorLock);
                m_entities.push_back (e);
            }

            // Static task for background file reading
            static void* fileReaderTask (void* arg);

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
                m_barrier->wait();
                int localThreadId = m_threadIndexer.fetch_add(1) % 32;

                while (!m_done && !this->msg_queue()->deactivated())
                {
                    qint64 now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

                    // Use tryacquire() to prevent the "Mutex Storm" from blocking the GUI
                    if (m_vectorLock.tryacquire() == 0)
                    { 
                        for (size_t i = (size_t)localThreadId; i < m_entities.size(); i += 32)
                        {
//                            if (m_done) break;
                            m_entities[i]->updatePhysics (now, Globe::m_liveOffset);
                        }

                        m_vectorLock.release(); 
                    }
                    else
                    {
                        // If the lock is busy, yield immediately to let the GUI or Parser in
                        ACE_Thread::yield();
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
            static void* parsingTask (void* arg);
            void addBatch (const std::vector<BaseEntity*>&& newEntities);
            void removeByGroup (const QString& groupKey);
        
        public slots: // Or just public:
            void processTleData (const QString& data, const QString& group);
    };
}
