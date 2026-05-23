#pragma once

#ifndef ENTITYMANAGER_HXX
#define ENTITYMANAGER_HXX

// SimCore/EntityManager.hxx
#include "BaseEntity.hxx"
#include "Tracking.hxx"
#include <ace/Task.h>
#include <ace/Barrier.h>
#include "ace/Thread.h"
#include <vector>
#include <algorithm>
#include "SystemCapabilities.hxx"

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

            ::SchedulingTier m_selectedTier;
            std::vector<HardwareCore> m_hardwareCorePool;

            std::unordered_set<QString> m_activeIds; // Fast lookup for duplicates

        public:
            static ACE_Thread_Mutex m_vectorLock; // Protects the vector itself
            static bool m_updatingEntities;
            Objects::Tracking* m_tracker;

            /************* Functions ******************/

            static EntityManager* instance();

            void startSimulation (int numThreads);
            void stopSimulation();

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
            virtual int svc() override;

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
