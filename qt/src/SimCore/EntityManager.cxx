#include "EntityManager.hxx"
#include "Satellite.hxx"
#include "MainWindow.hxx"

namespace SimCore
{
    ACE_Thread_Mutex SimCore::EntityManager::m_vectorLock;

    bool EntityManager::m_updatingEntities = false;

    EntityManager* EntityManager::s_instance = nullptr;

    EntityManager* EntityManager::instance()
    {
        return s_instance;
    }


    void EntityManager::onDataReceived (const QString& data)
    {
        // Spawn a dedicated, detached thread just for the heavy parsing
        ACE_Thread_Manager::instance()->spawn ((ACE_THR_FUNC)EntityManager::parsingTask, 
                                               new QString (data), // Pass the data heap-allocated
                                               THR_DETACHED
                                              );
    }
    

    void EntityManager::processTleData (const QString& info, const QString& group)
    {
//        SIM_LOG (LM_INFO, "EntityManager::processTleData");

        if (info.startsWith ("FILE_READY:"))
        {
            // USE THE FILE READER TASK
            // This task opens the file path (info.mid(11)) and reads the lines
            auto* data = new FileTaskData { info.mid (11), group };
            ACE_Thread_Manager::instance()->spawn ((ACE_THR_FUNC)EntityManager::fileReaderTask, 
                                                   data, 
                                                   THR_DETACHED | THR_NEW_LWP
                                                  );
        }
        else
        {
            // USE THE PARSING TASK
            // This task treats 'info' as the raw TLE text block
            auto* data = new ParsingTaskData { info, group };
            ACE_Thread_Manager::instance()->spawn ((ACE_THR_FUNC)EntityManager::parsingTask, 
                                                   data, 
                                                   THR_DETACHED | THR_NEW_LWP
                                                  );
        }
    }


    // Static helper for the dedicated parsing thread
    void* EntityManager::parsingTask (void* arg)
    {
        auto* taskData = static_cast<ParsingTaskData*> (arg);
        QString group = taskData->group;
        QString rawData = taskData->data;

//        SIM_LOG (LM_DEBUG, "EntityManager::parsingTask: Starting parsing task...");
        
        // Split by any newline variation (\r\n, \n, \r)
        //QStringList lines = rawData.split('\n', Qt::SkipEmptyParts);
        QStringList lines = rawData.split (QRegularExpression ("(\r\n|\n|\r)"), Qt::SkipEmptyParts);
        
        std::vector<BaseEntity*> newSats;
        int parsedCount = 0;

        // TLEs are 3-line blocks. We must ensure we have a full triplet.
        for (int i = 0; i + 2 < lines.size();)
        {
            QString name = lines[i].trimmed();
            QString l1 = lines[i+1];
            QString l2 = lines[i+2];

            // Check if l1 starts with '1 ' and l2 starts with '2 '
            // This validates we haven't lost our place in the 3-line sequence
            if (l1.startsWith ("1 ") && l2.startsWith ("2 "))
            {
                // Force exactly 69 characters by padding with spaces
                std::string line1 = l1.leftJustified (69, ' ').left (69).toStdString();
                std::string line2 = l2.leftJustified (69, ' ').left (69).toStdString();

                try
                {
                    auto* sat = new Space::Satellite (name, line1, line2, group);
                    newSats.push_back (sat);
                    parsedCount++;
                }
                catch (...)
                {
//                    SIM_LOG (LM_WARNING, "EntityManager::parsingTask: Skipping malformed satellite.");
                }

                i += 3; // Move to next triplet
            }
            else
            {
                std::cout << "OUT OF SYNC!" <<std::endl;

                // We are out of sync! Move forward one line at a time until we find a '1 '
                i++; 
            }
        }

        if (EntityManager::instance())
        {
            EntityManager::instance()->addBatch (std::move (newSats));
        }

        return nullptr;
    }


    void EntityManager::addBatch (const std::vector<BaseEntity*>&& newEntities)
    {
        if (newEntities.empty())
        {
            return;
        }

        {
            ACE_GUARD(ACE_Thread_Mutex, mon, m_vectorLock);

            for (auto* entity : newEntities)
            {
                if (!entity) continue;

                auto* sat = dynamic_cast<Space::Satellite*> (entity);

                if (!sat)
                {
                    delete entity;
                    continue;
                }

                QString id = sat->getNoradId();

                if (m_activeIds.find (id) == m_activeIds.end())
                {
                    m_activeIds.insert (id);
                    m_entities.push_back (sat);
                }
                else
                {
                    delete entity;   // duplicate
                }
            }

            m_totalActiveEntities = static_cast<int> (m_entities.size());
        } // lock released here

        // Only sync if the buffer is ready
        if (m_persistentBufferPtr)
        {
            synchronizeSatellitesToVRAM();
        }

//        SIM_LOG (LM_INFO, QString ("Added batch. Total satellites now: %1").arg (m_entities.size()));
    }


    void* EntityManager::fileReaderTask (void* arg)
    {
 //       SIM_LOG (LM_INFO, "EntityManager::fileReaderTask");

        // 1. Capture and wrap in a smart pointer immediately for safety
        std::unique_ptr<FileTaskData> data (static_cast<FileTaskData*> (arg));

        if (!data) return nullptr;

        QFile file (data->path);

        if (!file.open (QIODevice::ReadOnly | QIODevice::Text)) return nullptr;

        QTextStream in (&file);
        std::vector<BaseEntity*> batch;
        
        while (!in.atEnd())
        {
            QString name = in.readLine().trimmed();

            if (name.isEmpty()) continue;

            QString l1 = in.readLine().trimmed();
            QString l2 = in.readLine().trimmed();

            if (l1.length() >= 69 && l2.length() >= 69 && l1.startsWith("1 ") && l2.startsWith ("2 "))
            {
                // Force exactly 69 characters by padding with spaces
                std::string line1 = l1.leftJustified (69, ' ').left (69).toStdString();
                std::string line2 = l2.leftJustified (69, ' ').left (69).toStdString();

                if (line1[0] != '1' || line2[0] != '2') continue;

                try
                {                    
                    // Add to batch ONLY if the propagator initializes successfully
                    batch.push_back (new Space::Satellite (name, line1, line2, data->group));
                }
                catch (const std::exception& e)
                {
//                    SIM_LOG (LM_ERROR, QString ("Propagator init failed for %1: %2").arg (name).arg (e.what()));
                }
            }

            if (batch.size() >= 1000)
            {
                EntityManager::instance()->addBatch (std::move (batch));
                batch.clear();
            }
        }

        if (!batch.empty())
        {
            EntityManager::instance()->addBatch (std::move (batch));
        }

//        ACE_OS::sleep (ACE_Time_Value (1, 0));


        // THE HANDSHAKE: Before the unique_ptr 'data' is destroyed and the 
        // thread stack is reclaimed, ensure the Manager is done.
        {
            ACE_GUARD_RETURN (ACE_Thread_Mutex, mon, EntityManager::m_vectorLock, nullptr);
            // Simply acquiring the lock once here acts as a memory barrier
        }

        // Safe final sync with buffer check
        auto* mgr = EntityManager::instance();

        if (mgr && mgr->m_persistentBufferPtr)
        {
            mgr->synchronizeSatellitesToVRAM();
        }

        return nullptr; // data (unique_ptr) is deleted here automatically
    }


    void EntityManager::startSimulation (int numThreads)
    {
        m_tracker = new Objects::Tracking();
        s_instance = this;
        m_done = false;
        m_threadIndexer = 0;
        m_numThreads = numThreads;
        
        // 1. DETERMINE SYSTEM HARDWARE AND EXECUTION PERMISSIONS
        // The resulting list can be used for spawning other threads and tasks later
        m_selectedTier = SystemCapabilities::AnalyzeTopologyAndPermissions (m_hardwareCorePool);

        if (m_hardwareCorePool.size() < m_numThreads)
        {
            SIM_LOG (LM_INFO, QString ("Found less than %1 cores available, setting thread count to %2")
                                       .arg (m_numThreads)
                                       .arg (m_hardwareCorePool.size())
                                      );

            m_numThreads = m_hardwareCorePool.size();
        }

        // 2. VERBOSE LOGGING FOR ENTERPRISE ENVIRONMENT MANAGEMENT
        QString tierName;

        switch (m_selectedTier)
        {
            case SchedulingTier::RealTimeAndAffinity:
                tierName = "Tier 1: [REAL-TIME SCHED_FIFO + INTUITIVE HARDWARE CORE PINNING]";
                break;

            case SchedulingTier::AffinityOnly:
                tierName = "Tier 2: [STANDARD SCHEDULER TIMESHARING + INTUITIVE HARDWARE CORE PINNING]";
                break;

            case SchedulingTier::StandardFallback:
                tierName = "Tier 3: [STANDARD FALLBACK - SINGLE CORE OR OS CONTEXT ACCESS BLOCKED]";
                break;
        }

        SIM_LOG (LM_INFO, QString ("System Initialization: Selected Operating Framework: %1").arg (tierName));
        SIM_LOG (LM_INFO, QString ("Detected Total Hardware Units: %1 available processing paths.").arg (m_numThreads));

        // Inside EntityManager::startSimulation() right after parsing topology
    SIM_LOG (LM_INFO, QString ("HETEROGENEOUS POOL MAPPING SUMMARY:"));
    SIM_LOG (LM_INFO, QString ("  -> Main Thread / UI: Reserved exclusively for Core 0"));

    size_t physicalCount = 0;
    size_t siblingCount  = 0;

    for (const auto& core : m_hardwareCorePool)
    {
        if (core.isHTSibling) siblingCount++;
        else physicalCount++;
    }

    SIM_LOG (LM_INFO, QString ("  -> Pool 1 (SGP4 Workers): %1 Physical Cores allocated (Cores 1-15)")
             .arg (physicalCount - 1));
    SIM_LOG (LM_INFO, QString ("  -> Pool 2 (Path Predictors): %1 Sibling Cores allocated (Cores 17-31)")
             .arg (siblingCount - 1));

        // 3. SYNCHRONIZE BARRIER LIFECYCLE HANDSHAKE
        m_barrier = new ACE_Barrier (m_numThreads + 1);
        
        // Launch threads as dedicated kernel-level entities (THR_BOUND)
        this->activate (THR_NEW_LWP | THR_JOINABLE | THR_BOUND, m_numThreads);
        
        m_barrier->wait();
        SIM_LOG (LM_DEBUG, QString ("All %1 SGP4 Simulation threads pinned, scaled, and synchronized.").arg (m_numThreads));
    }


    void EntityManager::removeByGroup (const QString& groupKey)
    {
        std::vector<BaseEntity*> toDelete;

        SIM_LOG(LM_INFO, QString("Removing group '%1'...").arg(groupKey));

        {
            ACE_GUARD(ACE_Thread_Mutex, mon, m_vectorLock);

            auto it = std::remove_if(m_entities.begin(), m_entities.end(),
                                     [&](BaseEntity* e) -> bool
            {
                if (!e) return false;
                auto* sat = dynamic_cast<Space::Satellite*>(e);
                if (sat && sat->getGroup() == groupKey)
                {
                    m_activeIds.erase(sat->getNoradId());
                    toDelete.push_back(e);
                    return true;
                }
                return false;
            });

            m_entities.erase(it, m_entities.end());
            m_totalActiveEntities = static_cast<int>(m_entities.size());

            if (m_persistentBufferPtr)
            {
                clearSatelliteBufferZone();          // safe inside lock
            }
        } // mutex released here

        // Delete outside lock
        for (auto* e : toDelete)
            delete e;

        // Re-sync remaining satellites safely
        if (m_persistentBufferPtr)
        {
            synchronizeSatellitesToVRAM();
        }

        SIM_LOG(LM_INFO, QString("Group '%1' removed. %2 satellites remaining.")
                .arg(groupKey).arg(m_entities.size()));
    }


    int EntityManager::svc() 
    {
        // Initial startup sync boundary handshake
        m_barrier->wait();

        // Secure a unique, bound-safe ID matching your active worker pool size
        int localThreadId = m_threadIndexer.fetch_add(1) % m_numThreads;
        ACE_thread_t nativeThreadHandle = ACE_OS::thr_self();

        size_t availableCores = m_hardwareCorePool.size();

        // =========================================================================
        // STEP 1: HETEROGENEOUS TOPOLOGY WORKLOAD PARTITIONING
        // =========================================================================
        if (m_selectedTier != SchedulingTier::StandardFallback && availableCores > 0)
        {
            try
            {
                int targetCpuId = 0;
                WorkloadType myWorkload = WorkloadType::SGP4_PROPAGATOR;

                // Dynamically assign thread types based on your application lifecycle allocation
                // Example: Split pool so higher index blocks handle trajectory predictions
                if (localThreadId >= (m_numThreads / 2))
                {
                    myWorkload = WorkloadType::PATH_PREDICTOR;
                }

                std::vector<int> primaryPhysicalCores;
                std::vector<int> hyperthreadedSiblingCores;

                // Divide the unfiltered pool into independent hardware computing pools
                for (size_t i = 0; i < availableCores; ++i)
                {
                    if (m_hardwareCorePool.at (i).isHTSibling)
                    {
                        hyperthreadedSiblingCores.push_back (m_hardwareCorePool.at (i).logicalId);
                    }
                    else
                    {
                        primaryPhysicalCores.push_back (m_hardwareCorePool.at (i).logicalId);
                    }
                }

                // CORE ROUTING ENGINE
                if (myWorkload == WorkloadType::SGP4_PROPAGATOR && !primaryPhysicalCores.empty())
                {
                    // SGP4 Threads: Pinned to physical cores, skipping Core 0 to protect graphics
                    size_t poolOffset = (static_cast<size_t> (localThreadId) % (primaryPhysicalCores.size() - 1)) + 1;
                    targetCpuId = primaryPhysicalCores.at (poolOffset);
                } 
                else if (myWorkload == WorkloadType::PATH_PREDICTOR && !hyperthreadedSiblingCores.empty())
                {
                    // Path Prediction: Maps to HT sibling units to share FPU execution blocks
                    size_t poolOffset = (static_cast<size_t> (localThreadId) % (hyperthreadedSiblingCores.size() - 1)) + 1;
                    targetCpuId = hyperthreadedSiblingCores.at (poolOffset);
                } 
                else
                {
                    // Fallback to basic linear stride if HT is completely disabled in system BIOS
                    targetCpuId = m_hardwareCorePool.at (static_cast<size_t>(localThreadId) % availableCores).logicalId;
                }

                cpu_set_t cpuset;
                CPU_ZERO (&cpuset);
                CPU_SET (targetCpuId, &cpuset);
                ::pthread_setaffinity_np (nativeThreadHandle, sizeof (cpu_set_t), &cpuset);

                // =========================================================================
                // STEP 2: REAL-TIME ESCALATION & SCHEDULER TUNING
                // =========================================================================
                if (m_selectedTier == SchedulingTier::RealTimeAndAffinity)
                {
                    struct sched_param param;
                    
                    // MISSILE COMMAND PRIORITY HIERARCHY:
                    // Intercept path calculations take precedence over background satellite rendering
                    if (myWorkload == WorkloadType::PATH_PREDICTOR)
                    {
                        param.sched_priority = 35; // Higher real-time tier
                    }
                    else
                    {
                        param.sched_priority = 20; // Standard background real-time tier
                    }

                    int rtStatus = ::pthread_setschedparam (nativeThreadHandle, SCHED_FIFO, &param);

                    if (rtStatus != 0)
                    {
                        // System-level block caught: drop back down to safe time-sharing niceness
#if defined (__linux__)
                        ::setpriority (PRIO_PROCESS, 0, 5);
#endif
                    }
                } 
                else
                {
                    // Tier 2 Fallback: Apply relative niceness under SCHED_OTHER
#if defined (__linux__)
                    int targetNice = (myWorkload == WorkloadType::PATH_PREDICTOR) ? 2 : 6;
                    ::setpriority (PRIO_PROCESS, 0, targetNice);
#endif
                }
            } 
            catch (const std::out_of_range& e)
            {
                SIM_LOG (LM_ERROR, QString ("Core allocation exception on Thread %1.").arg (localThreadId));
            }
        }

        // =========================================================================
        // 2. DATA PROCESSING PIPELINE
        // =========================================================================

        size_t currentSize;
        size_t satCount;
        size_t missileCount;

        // Establish strict architectural division bounds based on your thread type assignment
        int halfPool = m_numThreads / 2; // Split threshold (e.g., index 16)

        float frameDeltaSeconds;

        std::chrono::time_point<std::chrono::high_resolution_clock> now;

        BaseEntity* entity;

        QVector3D realPosition;

        std::chrono::system_clock::duration duration;

        qint64 msecs;

        std::chrono::microseconds m_duration;

        std::chrono::time_point<std::chrono::high_resolution_clock> lastTickTime = std::chrono::high_resolution_clock::now();

        while (!m_done)// && !this->msg_queue()->deactivated())
        {
            now = std::chrono::high_resolution_clock::now();

            // SGP4 times
            duration = now.time_since_epoch();
            msecs = std::chrono::duration_cast<std::chrono::milliseconds> (duration).count();


            // Missile times
            m_duration = std::chrono::duration_cast<std::chrono::microseconds> (now - lastTickTime);
            lastTickTime = now;

            // Convert microseconds to fractional elapsed seconds parameter, passed to missile physicis engine
            frameDeltaSeconds = static_cast<float>(m_duration.count()) / 1000000.0f;

            currentSize = m_entities.size();
            satCount     = m_entities.size();
            missileCount = m_missiles.size();

////            SIM_LOG (LM_DEBUG, QString ("Aquire lock %1").arg (localThreadId));

            // Use tryacquire() to prevent the "Mutex Storm" from blocking the GUI
            //if (m_vectorLock.tryacquire() == 0)
            {
                // =====================================================================
                // WORKLOAD DIVISION 1: SGP4 SATELLITE PROPAGATION (PHYSICAL CORES)
                // =====================================================================
                // Only threads 1 to 15 handle raw satellite orbit computations
                if (localThreadId < halfPool && satCount > 0 && this->m_persistentBufferPtr != nullptr)
                {
////                    SIM_LOG (LM_DEBUG, QString ("Loop updatePhysics %1").arg (localThreadId));

                    if (m_vectorLock.tryacquire() == 0)
                    {
                        for (size_t i = static_cast<size_t>(localThreadId); i < satCount; i += static_cast<size_t>(halfPool))
                        {
                            entity = m_entities[i];

                            if (!m_entities.empty() && entity)
                            {
                                m_entities[i]->updatePhysics (msecs, Globe::m_liveOffset);

                                // ZERO-COPY INJECTION: Stream calculations straight to the GPU pointer.
                                // Because each thread manages separate indices, they write safely with ZERO lock contention.
                                realPosition = entity->getPosition();
                                
                                m_persistentBufferPtr[i].position = QVector4D (realPosition.x(),
                                                                               realPosition.y(),
                                                                               realPosition.z(),
                                                                               1.0f
                                                                              );
                                this->m_persistentBufferPtr[i].velocity.setW (1.0f); // Status flag: Active Satellite
                            }
                        }

                        SIM_LOG (LM_DEBUG, QString ("Release SAT lock %1").arg (localThreadId));
                        m_vectorLock.release();
                    }
                    else
                    {
                        // If the lock is busy, yield immediately to let the GUI or Parser in
                        ACE_Thread::yield();
                    }
                }

                // =====================================================================
                // WORKLOAD DIVISION 2: MISSILE ARC TRAJECTORIES (HYPERTHREADED SIBLINGS)
                // =====================================================================
                // Only threads 16 to 31 handle guided weapon paths and missile trail geometry
                if (localThreadId >= halfPool && missileCount > 0)
                {
                    // Calculate a localized, zero-based indexing offset for the missile loops (0 to 15)
                    size_t missileThreadOffset = static_cast<size_t> (localThreadId - halfPool);

                    if (m_vectorLock.tryacquire() == 0)
                    {
                        // Stride explicitly by the width of the path predictor pool (top half of pool)
                        for (size_t m = missileThreadOffset; m < missileCount; m += static_cast<size_t> (halfPool))
                        {
                            if (m < m_missiles.size())
                            {
                                Objects::GuidedMissile* missile = m_missiles[m];
                                
                                if (missile)
                                {
                                    if (missile->isActive())
                                    {
                                        missile->updateMissileFromGPU (m_persistentBufferPtr[missile->getSsboIndex()]);
                                        bool isDetected =
                                            missile->isInsideSensorVolume (missile->getPosition(),
                                                                           m_tracker->m_filterActive,
                                                                           m_tracker->m_filterAnchor,
                                                                           m_tracker->m_detectionRange -
                                                                           Globe::globeRadius
                                                                          );
                                        // 1. Advance linear trajectory curves using CPU mathematical tracking
                                        missile->updatePhysics (m_persistentBufferPtr[missile->getSsboIndex()],
                                                                frameDeltaSeconds, isDetected
                                                               );
                                    }
                                    else
                                    {
                                        missile->deactivate();
                                    }
                                }
                            }
                        }

                        SIM_LOG (LM_DEBUG, QString ("Release MIS lock %1").arg (localThreadId));
                        m_vectorLock.release();
                    }
                    else
                    {
                        // If the lock is busy, yield immediately to let the GUI or Parser in
                        ACE_Thread::yield();
                    }
                }
            }

            if (::Config::getInstance().THREAD_SLEEP_TIME > 0)
            {
                ACE_OS::sleep (ACE_Time_Value (0, ::Config::getInstance().THREAD_SLEEP_TIME));
            }
            else
            {
                ACE_OS::sleep (ACE_Time_Value (0, ::Config::getInstance().DEFAULT_THREAD_SLEEP));
            }
        }

        return 0;
    } // END: svc()


    void EntityManager::stopSimulation()
    {
        SIM_LOG (LM_INFO, QString ("Initiating Multi-Threaded Simulation Shutdown Sequence..."));
        
        // 1. RAISE TRANSITION COOPERATIVE LIFECYCLE FLAGS
        m_done = true;
        
        // Deactivate the underlying message queue to wake any threads blocked on it
        this->msg_queue()->deactivate();

        // 2. EXPLICITLY RETURN AFFINITY RESOURCING BACK TO THE GENERAL OPERATING POOL
        // On systems running true real-time priorities (SCHED_FIFO), resetting the 
        // scheduling parameters on shutdown ensures the cores are cleanly returned 
        // to standard OS management, preventing lockups.
        if (m_selectedTier == SchedulingTier::RealTimeAndAffinity)
        {
            long totalCores = ::sysconf (_SC_NPROCESSORS_ONLN);
            cpu_set_t fullSystemMask;
            CPU_ZERO (&fullSystemMask);

            for (int i = 0; i < totalCores; ++i)
            {
                CPU_SET (i, &fullSystemMask);
            }

            struct sched_param standardParam;
            standardParam.sched_priority = 0;

            // Reset the main thread's parameters safely
            ::pthread_setschedparam (ACE_OS::thr_self(), SCHED_OTHER, &standardParam);
            ::pthread_setaffinity_np (ACE_OS::thr_self(), sizeof (cpu_set_t), &fullSystemMask);
        }

        // 3. REAP THE COMPUTE VECTOR THREAD POOL
        // This blocks the shutdown sequence until all worker threads break 
        // their loops and exit cleanly, avoiding memory leaks or dangling pointers.
        this->wait(); 

        // 4. CLEANUP DYNAMIC COMPONENT MEMORY ALLOCATIONS
        if (m_barrier)
        {
            delete m_barrier;
            m_barrier = nullptr;
        }

        SIM_LOG (LM_INFO, QString ("Simulation Shutdown Finalized Successfully. All threads reaped."));
    }


    void EntityManager::handleSatelliteExplosion (size_t targetIndex, const QVector3D& impactPos)
    {
        // 1. Convert the parent satellite to a piece of kinetic debris instantly
        m_persistentBufferPtr[targetIndex].metadata.setW (DataObjects::TYPE_KINETIC_DEBRIS);
        m_persistentBufferPtr[targetIndex].metadata.setX (5.0f); // 5 seconds of lifespan before fading
        
        // 2. Spawn surrounding shrapnel fragments using adjacent empty array slots
        int fragmentsSpawned = 0;
        size_t poolSize = m_entities.size(); // Sized up to 50,000 max capacity

        for (size_t i = 0; i < poolSize && fragmentsSpawned < 25; ++i)
        {
            // Locate an inactive or dead memory slot inside the persistent array
            if (m_persistentBufferPtr[i].metadata.w() == DataObjects::TYPE_DEAD_SLOT)
            {
                // Initialize position at the exact point of impact
                m_persistentBufferPtr[i].position = QVector4D (impactPos.x(), impactPos.y(), impactPos.z(), 1.0f);
                
                // Generate a random outward kinetic blast velocity vector
                QVector3D shrapnelVel = CalculateExplosionVector(); 
                m_persistentBufferPtr[i].velocity = QVector4D (shrapnelVel.x(), shrapnelVel.y(), shrapnelVel.z(), 1.0f);
                
                // Flag it as kinetic debris so the GPU Compute shader takes over next frame
                m_persistentBufferPtr[i].metadata.setX (3.0f + (rand() % 100 / 50.0f)); // Randomized lifespan
                m_persistentBufferPtr[i].metadata.setW (DataObjects::TYPE_KINETIC_DEBRIS);
                
                fragmentsSpawned++;
            }
        }
    }


    QVector3D EntityManager::CalculateExplosionVector()
    {
        // 1. Fetch an aligned, randomized 3D unit direction vector
        QVector3D blastDirection = Utility::randomSphericalVector();

        // 2. Compute a randomized speed scalar using clean relative bounds
        // (Tweak these variables to control how fast the debris shards expand)
        float minSpeed = 0.01f;
        float maxSpeed = 0.03f;
        float blastVelocity = Utility::randomFloat (minSpeed, maxSpeed);

        // Return the completed directional move velocity vector
        return blastDirection * blastVelocity;
    }


    void EntityManager::injectTestThreat (const QVector3D& launchOrigin, const QVector3D& impactTarget)
    {
        // Acquire an exclusive write lock to modify the vector safely
        // (Briefly pauses your physics loops during the pointer push)
        if (m_vectorLock.acquire_write() == 0)
        {
            size_t nextSsboSlot = m_missiles.size();

            // Safety limit: Don't overflow your allocated 500-missile buffer bounds
            if (nextSsboSlot < static_cast<size_t>(::Config::getInstance().MAX_MISSILES))
            {
                int uniqueId = static_cast<int> (nextSsboSlot) + 1000;
                
                // Instantiate a new threat entity
                Objects::GuidedMissile* threat = new Objects::GuidedMissile (uniqueId, nextSsboSlot,
                                                                             launchOrigin, impactTarget);
                threat->setTargetMode (TargetMode::ANTI_SATELLITE_STRIKE);
                
                m_missiles.push_back (threat);

                SIM_LOG (LM_INFO, QString ("TACTICAL INJECTOR: Spawned Threat MSL-%1 into SSBO Slot %2")
                         .arg (uniqueId)
                         .arg (nextSsboSlot)
                        );
            }
            else
            {
                SIM_LOG (LM_WARNING, QString ("TACTICAL INJECTOR: Maximum missile buffer capacity (%1) reached!")
                         .arg (::Config::getInstance().MAX_MISSILES)
                        );
            }

            m_vectorLock.release();
        }
        else
        {
            SIM_LOG (LM_CRITICAL, "FAILED TO GET VECTOR WRITE LOCK");
        }
    }


    void EntityManager::injectGpuThreat (const QVector3D& origin, const QVector3D& target)
    {
        if (m_vectorLock.acquire() == 0)
        {
            const size_t maxMissiles = static_cast<size_t> (::Config::getInstance().MAX_MISSILES);

            // Honor the strict catalog firewall boundary to protect the SGP4 satellite tracks
            size_t tacticalStartSlot  = static_cast<size_t> (::Config::getInstance().MAX_SAT_BUFF_SZ);

            const size_t totalSlots = static_cast<size_t> (::Config::getInstance().MAX_OBJECTS);

            if (m_missiles.size() < maxMissiles)
            {
                // Find first available dead slot in tactical zone
                for (size_t i = tacticalStartSlot; i < totalSlots; ++i)
                {
                    if (m_persistentBufferPtr[i].metadata.w() == 0.0f)   // Dead slot
                    {
                        bool isInterceptor = (origin.length() < (Globe::globeRadius * 1.5f));
                        float typeId = isInterceptor ? 3.0f : 4.0f;

                        float targetMach = isInterceptor ? 
                                           (::Config::getInstance().MAX_THAAD_SPD * 1.5f) : 
                                           ::Config::getInstance().MAX_ICBM_SPD;

                        float glUnitsPerSecond = targetMach * Globe::glScaleFactor;

                        // === GPU-side injection ===
                        m_persistentBufferPtr[i].position = QVector4D (origin.x(), origin.y(), origin.z(), 1.0f);
                        QVector3D dir = (target - origin).normalized();
                        m_persistentBufferPtr[i].velocity = QVector4D (dir.x(), dir.y(), dir.z(), glUnitsPerSecond);

                        m_persistentBufferPtr[i].metadata.setX (120.0f);           // lifespan
                        m_persistentBufferPtr[i].metadata.setY (0.0f);
                        m_persistentBufferPtr[i].metadata.setZ (20.0f);            // ballistic state
                        m_persistentBufferPtr[i].metadata.setW (typeId);

                        // === CPU-side GuidedMissile object ===
                        int uniqueId = static_cast<int> (m_missiles.size()) + 1000;
                        Objects::GuidedMissile* missile = new Objects::GuidedMissile (uniqueId, i, origin, target);
                        missile->updateVelocity (m_persistentBufferPtr[i].velocity);

                        if (isInterceptor)
                        {
                            missile->setTargetMode (TargetMode::ANTI_SATELLITE_STRIKE);
                        }

                        m_missiles.push_back (missile);

                        SIM_LOG (LM_INFO, QString ("TACTICAL INJECTOR: Spawned %1 MSL-%2 into SSBO Slot %3")
                                 .arg (isInterceptor ? "THAAD" : "ICBM")
                                 .arg (uniqueId)
                                 .arg (i)
                               );

                        m_vectorLock.release();
                        return;
                    }
                }
            }
            else
            {
                SIM_LOG (LM_WARNING, "Missile limit reached - dropping spawn");
            }
        }
        else
        {
            SIM_LOG (LM_WARNING, "injectGpuThreat: Failed to acquire lock");
        }

        SIM_LOG (LM_WARNING, "No available missile slots");
        m_vectorLock.release();
    } // END: injectGpuThreat (const QVector3D& origin, const QVector3D& target)


    void EntityManager::clearSatelliteBufferZone()
    {
        if (!m_persistentBufferPtr) 
            return;

        const size_t satCeiling = static_cast<size_t> (::Config::getInstance().MAX_SAT_BUFF_SZ);
        const size_t bytes = satCeiling * sizeof (DataObjects::GpuEntityData);

        std::memset (m_persistentBufferPtr, 0, bytes);
    }


    void EntityManager::initializeSatelliteBufferSlots()
    {
        if (!m_persistentBufferPtr)
        {
            SIM_LOG(LM_CRITICAL, "initializeSatelliteBufferSlots() - m_persistentBufferPtr is null!");
            return;
        }

        const size_t satCeiling = static_cast<size_t> (::Config::getInstance().MAX_SAT_BUFF_SZ);
        const size_t totalSlots = static_cast<size_t> (::Config::getInstance().MAX_OBJECTS);

        // Satellite zone
        for (size_t i = 0; i < satCeiling; ++i)
        {
            auto& slot = m_persistentBufferPtr[i];
            slot.position = QVector4D (0.0f, 0.0f, 0.0f, 1.0f);
            slot.velocity = QVector4D (0.0f, 0.0f, 0.0f, 1.0f);
            slot.metadata = QVector4D (9999.0f, 1.0f, 10.0f, DataObjects::TYPE_SGP4_SATELLITE);
            slot.padding  = QVector4D();
        }

        // Tactical zone (zero)
        std::memset (m_persistentBufferPtr + satCeiling, 0, 
                     (totalSlots - satCeiling) * sizeof (DataObjects::GpuEntityData)
                    );

        SIM_LOG(LM_INFO, QString ("SSBO zones re-initialized (%1 sat slots)").arg (satCeiling));
    }


    void EntityManager::synchronizeSatellitesToVRAM()
    {
        if (!m_persistentBufferPtr)
            return;

        ACE_GUARD (ACE_Thread_Mutex, mon, m_vectorLock);
        clearSatelliteBufferZone();

        const size_t maxSatSlots = static_cast<size_t> (::Config::getInstance().MAX_SAT_BUFF_SZ);
        const size_t writeCount  = std::min(m_entities.size(), maxSatSlots);

        for (size_t i = 0; i < writeCount; ++i)
        {
            BaseEntity* entity = m_entities[i];

            if (!entity)
            {
                continue;
            }

            DataObjects::GpuEntityData payload{};

            // Position
            QVector3D pos = entity->getPosition();
            payload.position = QVector4D (pos.x(), pos.y(), pos.z(), Globe::glScaleFactor);

            // Velocity + status
            QVector3D dir = entity->getVelocityDirection();
            payload.velocity = QVector4D (dir.x(), dir.y(), dir.z(), entity->getSpeed());

            // Metadata
            payload.metadata.setX (entity->getLifespan());
            payload.metadata.setY (entity->getMass());           // or getThrust()
            payload.metadata.setZ (entity->getStateId());
            payload.metadata.setW (DataObjects::TYPE_SGP4_SATELLITE);

            m_persistentBufferPtr[i] = payload;   // atomic 64-byte write
        }
    }


    void EntityManager::resetAllSimulationState()
    {
        ACE_GUARD(ACE_Thread_Mutex, mon, m_vectorLock);

        // Clear all live entities
        for (auto* e : m_entities)
            delete e;

        m_entities.clear();
        m_activeIds.clear();

        // Clear missiles
        for (auto* m : m_missiles)
            delete m;

        m_missiles.clear();

        m_totalActiveEntities = 0;
    }


    void EntityManager::fullRestartSimulation()
    {
 //       SIM_LOG (LM_INFO, "=== FULL SIMULATION RESTART INITIATED ===");

        // 1. Graceful shutdown of all worker threads
        stopSimulation();

        // 2. Reset all state while threads are dead
        resetAllSimulationState();

        // 3. Clear any remaining GPU data (optional but clean)
        if (m_persistentBufferPtr)
            std::memset (m_persistentBufferPtr, 0,
                         ::Config::getInstance().MAX_OBJECTS * sizeof(DataObjects::GpuEntityData)
                        );

 //       SIM_LOG (LM_INFO, "Simulation state fully reset.");
    }


    std::vector<Objects::GuidedMissile*> EntityManager::snapshotActiveMissiles()
    {
        std::vector<Objects::GuidedMissile*> result;

        if (m_vectorLock.tryacquire() == 0)
        {
            result.reserve (m_missiles.size());

            for (auto* m : m_missiles)
            {
                if (m && m->isActive())
                {
                    result.push_back (m);
                }
            }

            m_vectorLock.release();
        }

        return result;
    }
} // namespace SimCore
