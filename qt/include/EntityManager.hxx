#pragma once

#ifndef ENTITYMANAGER_HXX
#define ENTITYMANAGER_HXX

// SimCore/EntityManager.hxx
#include "BaseEntity.hxx"
#include "Tracking.hxx"
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
            std::atomic<int> m_threadIndexer {0};
            ACE_Barrier* m_barrier = nullptr;
            int m_numThreads;

            std::unordered_set<QString> m_activeIds; // Fast lookup for duplicates

        public:
            static ACE_Thread_Mutex m_vectorLock; // Protects the vector itself
            static bool m_updatingEntities;
            Objects::Tracking* m_tracker;

            /************* Functions ******************/

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
                // 1. CHOOSE A UNIQUE CORE ASSIGNMENT PATTERN (0 to 31)
                // Map this specific worker thread context to a clean, isolated integer index
                size_t localThreadId = m_threadIndexer.fetch_add (1);

                // Fetch the underlying native POSIX/OS thread descriptor
                ACE_thread_t nativeThreadHandle = ACE_OS::thr_self();

                // =========================================================================
                // 2. CROSS-PLATFORM CPU AFFINITY CONTROL (No Permissions Required)
                // =========================================================================
                long totalOnlineCores = ::sysconf (_SC_NPROCESSORS_ONLN);

                if (totalOnlineCores > 1) 
                {
                    // LEAVE CORE 0 OPEN: Shift target indexing by 1 so Core 0 stays completely
                    // free for your main Qt window and user interaction events (Thread 33)
                    size_t assignedCore = (localThreadId % (totalOnlineCores - 1)) + 1;

#if defined (__linux__)
                    cpu_set_t cpuset;
                    CPU_ZERO (&cpuset);
                    CPU_SET (assignedCore, &cpuset);

                    // Directly bind the thread to its target core under RHEL 10
                    int affinityStatus = ::pthread_setaffinity_np (nativeThreadHandle, sizeof (cpu_set_t), &cpuset);

                    if (affinityStatus != 0)
                    {
                        ACE_DEBUG ((LM_WARNING, "Worker %i: Core %i affinity pinning rejected.",
                                    localThreadId, assignedCore));
                    }

#elif defined (__sun) || defined (sun)
                    // Solaris explicit thread core binding mechanism fallback
                    ::processor_bind (P_LWPID, P_MYID, static_cast<processorid_t>(assignedCore), NULL);

#elif defined (_AIX)
                    // IBM AIX explicit execution target core binding fallback
                    ::bindprocessor (BINDPROCESS, ::getpid(), static_cast<cpu_t>(assignedCore));
#endif
                }

                // =========================================================================
                // 3. SAFE PRIORITY MANAGEMENT (No Permissions Required)
                // =========================================================================
                // Unprivileged users cannot activate real-time scheduling (SCHED_FIFO).
                // Instead, we lower the "niceness" of the background worker tasks slightly.
                // This allows the OS to prioritize the main thread's 4K rendering loop.
#if defined (__linux__)
                // Nice values run from -20 (highest) to +19 (lowest).
                // Increasing niceness to +5 safely signals the kernel that this
                // background SGP4 math pool should yield resources to the rendering pipeline.
                int niceStatus = ::setpriority (PRIO_PROCESS, 0, 5);

                if (niceStatus != 0)
                {
                    ACE_DEBUG ((LM_DEBUG, "Worker %i: Nice value adjustment skipped.", localThreadId));
                }
#else
                // Cooperative user-space priority shifting for traditional UNIX environments
                int fallbackMinPrio = ACE_OS::thr_getminprio (THR_SCHED_DEFAULT);
                ACE_OS::thr_setprio (fallbackMinPrio);
#endif

                // =========================================================================
                // 4. THE SYNC HANDSHAKE BARRIER
                // =========================================================================
                // Block until all 32 background workers have finalized their core configurations.
                // Once the main thread hits its matching wait() call inside startSimulation(),
                // the barrier will break and execution will begin simultaneously.
                m_barrier->wait();

//                int localThreadId = m_threadIndexer.fetch_add (1) % ::Config::getInstance().MAX_THREADS;

                while (!m_done && !this->msg_queue()->deactivated())
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    auto duration = now.time_since_epoch();
                    qint64 msecs = std::chrono::duration_cast<std::chrono::milliseconds> (duration).count();

                    // Use tryacquire() to prevent the "Mutex Storm" from blocking the GUI
                    if (m_vectorLock.tryacquire() == 0)
                    { 
                        for (size_t i = (size_t)localThreadId; i < m_entities.size(); i += ::Config::getInstance().MAX_THREADS)
                        {
                            BaseEntity* entity = m_entities[i];

                            if (!m_entities.empty() && entity)
                            {
                                m_entities[i]->updatePhysics (msecs, Globe::m_liveOffset);
                            }
                        }

                        m_vectorLock.release();
                    }
                    else
                    {
                        // If the lock is busy, yield immediately to let the GUI or Parser in
                        ACE_Thread::yield();
                    }

//                    ACE_Thread::yield();
                    ACE_OS::sleep (ACE_Time_Value (0, ::Config::getInstance().THREAD_SLEEP_TIME));
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

#endif // ENTITYMANAGER_HXX
