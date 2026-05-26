#pragma once

#ifndef ENTITYMANAGER_HXX
#define ENTITYMANAGER_HXX

#include "BaseEntity.hxx"
#include "GuidedMissile.hxx"
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

            // The persistent container array tracking live interceptor entities
            std::vector<Objects::GuidedMissile*> m_missiles; 

        public:
            static ACE_Thread_Mutex m_vectorLock; // Protects the vector itself
            static bool m_updatingEntities;
            Objects::Tracking* m_tracker;
            int m_totalActiveEntities;

            // Persistent CPU-accessible pointer mapped directly to VRAM
            DataObjects::GpuEntityData* m_persistentBufferPtr = nullptr;
            DataObjects::PathVertex* m_persistentTrailPtr = nullptr; // Track inside your structures

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

            void setGpuBufferPointer (DataObjects::GpuEntityData* ptr)
            {
                this->m_persistentBufferPtr = ptr;
            }

            void setGpuTrailPointer (DataObjects::PathVertex* ptr)
            {
                this->m_persistentTrailPtr = ptr;
            }

            // Returns the exact size of the active guided weapons array
            int getActiveMissileCount() const
            {
                return static_cast<int> (this->m_missiles.size());
            }

            const std::vector<Objects::GuidedMissile*>& getMissiles() const
            {
                return m_missiles;
            }

            // Static helper for the parsing thread
            static void* parsingTask (void* arg);
            void addBatch (const std::vector<BaseEntity*>&& newEntities);
            void removeByGroup (const QString& groupKey);
            void handleSatelliteExplosion (size_t targetIndex, const QVector3D& impactPos);
            QVector3D CalculateExplosionVector();
            void injectTestThreat (const QVector3D& launchOrigin, const QVector3D& impactTarget);
            void injectGpuThreat (const QVector3D& origin, const QVector3D& target);
            void initializeSatelliteBufferSlots();
            void clearSatelliteBufferZone();

            Objects::GuidedMissile* getMissileAtIndex (int index)
            {
                return m_missiles.at (index);
            }

        public slots: // Or just public:
            void processTleData (const QString& data, const QString& group);
    };
}

#endif // ENTITYMANAGER_HXX
