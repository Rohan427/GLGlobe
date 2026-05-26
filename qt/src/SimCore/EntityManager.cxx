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
        }

        if (!EntityManager::instance())
        {
            return;
        }

        {
            // Lock the vector once for the whole batch
            ACE_GUARD (ACE_Thread_Mutex, mon, EntityManager::m_vectorLock);

            for (auto* entity : newEntities)
            {
                if (!entity) continue;

                auto* sat = static_cast<Space::Satellite*>(entity);
                QString id = sat->getNoradId();

                if (m_activeIds.find (id) == m_activeIds.end())
                {
                    m_activeIds.insert (id);
                    m_entities.push_back (sat);
                }
                else
                {
                    delete entity; // Already exists, discard the duplicate
                }
            }

            m_totalActiveEntities = static_cast<int> (m_entities.size());
        } // ACE_GUARD (ACE_Thread_Mutex, mon, m_vectorLock);
    }

    void* EntityManager::fileReaderTask (void* arg)
    {
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

        // THE HANDSHAKE: Before the unique_ptr 'data' is destroyed and the 
        // thread stack is reclaimed, ensure the Manager is done.
        {
            ACE_GUARD_RETURN (ACE_Thread_Mutex, mon, EntityManager::m_vectorLock, nullptr);
            // Simply acquiring the lock once here acts as a memory barrier
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
    SIM_LOG(LM_INFO, QString("HETEROGENEOUS POOL MAPPING SUMMARY:"));
    SIM_LOG(LM_INFO, QString("  -> Main Thread / UI: Reserved exclusively for Core 0"));

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

//        ACE_DEBUG((LM_INFO, ACE_TEXT("[TID:%t] Removing group: %s\n"), groupKey.toUtf8().constData()));
        std::cout << "removing group " << groupKey.toUtf8().constData() << std::endl;
        
        {
            ACE_GUARD (ACE_Thread_Mutex, mon, EntityManager::m_vectorLock);
            
            // Remove-Erase idiom: Fast and thread-safe inside the lock
            auto it = std::remove_if (m_entities.begin(), m_entities.end(), [&](BaseEntity* e)
            {
                auto* sat = static_cast<Space::Satellite*> (e);

                if (sat && sat->getGroup() == groupKey)
                {
                    m_activeIds.erase (sat->getNoradId()); // Remove from set
                    toDelete.push_back (e);
                    return true;
                }

                return false;
            });

            m_entities.erase (it, m_entities.end());

            this->clearSatelliteBufferZone();
        }

        // Free memory
        for (auto* e : toDelete)
        {
            if (!e) continue;
            delete e; 
        }

        m_totalActiveEntities = static_cast<int> (m_entities.size());

        SIM_LOG (LM_INFO, QString ("Removed group %1. Current count: %2").arg (groupKey).arg (m_entities.size()));
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
        auto lastTickTime = std::chrono::high_resolution_clock::now();

        while (!m_done)// && !this->msg_queue()->deactivated())
        {
            auto now = std::chrono::high_resolution_clock::now();

            // SGP4 times
            auto duration = now.time_since_epoch();
            qint64 msecs = std::chrono::duration_cast<std::chrono::milliseconds> (duration).count();


            // Missile times
            auto m_duration = std::chrono::duration_cast<std::chrono::microseconds> (now - lastTickTime).count();
            lastTickTime = now;
            // Convert microseconds to fractional elapsed seconds parameter, passed to missile physicis engine
            float frameDeltaSeconds = static_cast<float>(m_duration) / 1000000.0f;


            SIM_LOG (LM_DEBUG, QString ("Aquire lock %1").arg (localThreadId));

            // Use tryacquire() to prevent the "Mutex Storm" from blocking the GUI
            if (m_vectorLock.tryacquire() == 0)
            {
                size_t currentSize = m_entities.size();
                size_t satCount     = m_entities.size();
                size_t missileCount = m_missiles.size();

                // Establish strict architectural division bounds based on your thread type assignment
                int halfPool = m_numThreads / 2; // Split threshold (e.g., index 16)
                
                // =====================================================================
                // WORKLOAD DIVISION 1: SGP4 SATELLITE PROPAGATION (PHYSICAL CORES)
                // =====================================================================
                // Only threads 1 to 15 handle raw satellite orbit computations
                if (localThreadId < halfPool && satCount > 0 && this->m_persistentBufferPtr != nullptr)
                {
                    SIM_LOG (LM_DEBUG, QString ("Loop updatePhysics %1").arg (localThreadId));

                    for (size_t i = static_cast<size_t>(localThreadId); i < satCount; i += static_cast<size_t>(halfPool))
                    {
                        BaseEntity* entity = m_entities[i];

                        if (!m_entities.empty() && entity)
                        {
                            m_entities[i]->updatePhysics (msecs, Globe::m_liveOffset);

                            // ZERO-COPY INJECTION: Stream calculations straight to the GPU pointer.
                            // Because each thread manages separate indices, they write safely with ZERO lock contention.
                            QVector3D realPosition = entity->getPosition();
                            
                            m_persistentBufferPtr[i].position = QVector4D (realPosition.x(),
                                                                           realPosition.y(),
                                                                           realPosition.z(),
                                                                           1.0f
                                                                          );
                            this->m_persistentBufferPtr[i].velocity.setW (1.0f); // Status flag: Active Satellite
                        }
                    }
                }

                // =====================================================================
                // WORKLOAD DIVISION 2: MISSILE ARC TRAJECTORIES (HYPERTHREADED SIBLINGS)
                // =====================================================================
                // Only threads 16 to 31 handle guided weapon paths and missile trail geometry
                if (localThreadId >= halfPool && missileCount > 0 && this->m_persistentTrailPtr != nullptr)
                {
                    // Calculate a localized, zero-based indexing offset for the missile loops (0 to 15)
                    size_t missileThreadOffset = static_cast<size_t> (localThreadId - halfPool);

                    // Stride explicitly by the width of the path predictor pool (top half of pool)
                    for (size_t m = missileThreadOffset; m < missileCount; m += static_cast<size_t>(halfPool))
                    {
                        if (m < m_missiles.size())
                        {
                            Objects::GuidedMissile* missile = m_missiles[m];
                            
                            if (missile && missile->isActive())
                            {
                                // 1. Advance linear trajectory curves using CPU mathematical tracking
                                missile->updatePhysics (frameDeltaSeconds);
                                
                                // 2. ZERO-COPY TRAIL STREAMING: Push points directly to VRAM binding slot 1
                                if (this->m_persistentTrailPtr != nullptr)
                                {
                                    missile->updateTrailGeometry (this->m_persistentTrailPtr, frameDeltaSeconds);
                                }
                            }
                        }
                    }
                }

                SIM_LOG (LM_DEBUG, QString ("Release lock %1").arg (localThreadId));

                m_vectorLock.release();
            }
            else
            {
                // If the lock is busy, yield immediately to let the GUI or Parser in
                ACE_Thread::yield();
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
    }

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

                // =========================================================================
                // PRODUCTION TEST REPAIR: PRE-INITIALIZE PERSISTENT VRAM TRAIL DATA FIELDS
                // =========================================================================
                // Directly populate all 64 vertex layout blocks to force instant visibility on screen
                if (this->m_persistentTrailPtr != nullptr)
                {
                    size_t bufferStartOffset = nextSsboSlot * 64;
                    
                    for (size_t i = 0; i < 64; ++i)
                    {
                        // Pre-fill the coordinates with the launch origin position vector
                        this->m_persistentTrailPtr[bufferStartOffset + i].position = 
                            QVector4D (launchOrigin.x(), launchOrigin.y(), launchOrigin.z(), 1.0f);
                    }
                }
                
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
        if (m_vectorLock.acquire_write() == 0)
        {
            size_t totalSimulationCap = static_cast<size_t>(::Config::getInstance().MAX_OBJECTS);
            
            // Honor your strict catalog firewall boundary to protect your SGP4 satellite tracks
            size_t tacticalStartSlot  = static_cast<size_t>(::Config::getInstance().MAX_SAT_BUFF_SZ);

            for (size_t i = tacticalStartSlot; i < totalSimulationCap; ++i)
            {
                // Locate an open, dead memory slot inside the persistent array tracking grid
                if (this->m_persistentBufferPtr[i].metadata.w() == 0.0f)
                { // TYPE_DEAD_SLOT
                    
                    // Identify if this is a defensive launch or an incoming threat trajectory
                    bool isInterceptor = (origin.length() < (Globe::globeRadius * 1.05f)); 
                    float typeId = isInterceptor ? 3.0f : 4.0f; // 3.0 = Interceptor, 4.0 = Threat
                    
                    // Scale target velocities dynamically matching your configuration parameters
                    float targetMach = isInterceptor ? (::Config::getInstance().MAX_THAAD_SPD * 1.5f) : ::Config::getInstance().MAX_ICBM_SPD;
                    float glUnitsPerSecond = targetMach * Globe::glScaleFactor;

                    // =====================================================================
                    // VERIFIED ZERO-COPY VRAM CORES INJECTION
                    // =====================================================================
                    // Write the raw randomized origin position straight into the buffer slot!
                    this->m_persistentBufferPtr[i].position = QVector4D (origin.x(), origin.y(), origin.z(), 1.0f);
                    
                    // Compute the explicit normalized directional trajectory path vector
                    QVector3D travelDir = (target - origin).normalized();
                    this->m_persistentBufferPtr[i].velocity = QVector4D (travelDir.x(), travelDir.y(), travelDir.z(), glUnitsPerSecond);
                    
                    // Initialize metadata variables
                    this->m_persistentBufferPtr[i].metadata.setX (120.0f);                       // 120s flight clock
                    this->m_persistentBufferPtr[i].metadata.setY (0.0f);                         // No thrust variations
                    this->m_persistentBufferPtr[i].metadata.setZ (20.0f);                        // Jump straight to ballistic
                    this->m_persistentBufferPtr[i].metadata.setW (typeId);                       // Explicit macro identifier

                    std::string threat_name = isInterceptor ? "THAAD" : "ICBM";

                    SIM_LOG (LM_INFO, QString ("TACTICAL INJECTOR: Spawned Threat MSLinto SSBO Slot %2")
                             .arg (threat_name)
                             .arg (i)
                            );
                    
                    m_vectorLock.release();
                    return; // Escape immediately once memory slot configuration handshakes
                }
            }
            m_vectorLock.release();
        }
    }

    void EntityManager::initializeSatelliteBufferSlots()
    {
        if (this->m_persistentBufferPtr != nullptr)
        {
            size_t satBufferCeiling   = static_cast<size_t>(::Config::getInstance().MAX_SAT_BUFF_SZ);
            size_t totalSimulationCap = static_cast<size_t>(::Config::getInstance().MAX_OBJECTS);
            
            // Tier 1: Flag the satellite zone cleanly
            for (size_t i = 0; i < satBufferCeiling; ++i)
            {
                this->m_persistentBufferPtr[i].metadata.setW (1.0f); // 1.0 = TYPE_SGP4_SATELLITE
                this->m_persistentBufferPtr[i].velocity.setW (1.0f); // Active status
            }
            
            // =====================================================================
            // PRODUCTION REPAIR: FIREWALL THE REMAINDER OF THE 500K BOUNDARY
            // =====================================================================
            // Zeroing out the rest of the array ensures the vertex shader sees typeId == 0.0f
            // for unused slots and exits before running mathematical divisions on empty slots!
            for (size_t i = satBufferCeiling; i < totalSimulationCap; ++i)
            {
                this->m_persistentBufferPtr[i].position = QVector4D (0.0f, 0.0f, 0.0f, 0.0f);
                this->m_persistentBufferPtr[i].velocity = QVector4D (0.0f, 0.0f, 0.0f, 0.0f);
                this->m_persistentBufferPtr[i].metadata = QVector4D (0.0f, 0.0f, 0.0f, 0.0f); // setW(0.0f) = TYPE_DEAD_SLOT
            }
            
            SIM_LOG (LM_INFO, QString ("CATALOG INITIALIZATION SUCCESS: Sanitized 500,000 SSBO memory slots cleanly."));
        }
        else
        {
            SIM_LOG (LM_CRITICAL, QString ("CATALOG INITIALIZATION FAILED: Bufer not instantiated."));
        }
    }

    void EntityManager::clearSatelliteBufferZone()
    {
        if (this->m_persistentBufferPtr != nullptr)
        {
            size_t satBufferCeiling = static_cast<size_t> (::Config::getInstance().MAX_SAT_BUFF_SZ);
            
            // Scrub only your designated satellite catalog zone
            for (size_t i = 0; i < satBufferCeiling; ++i)
            {
                // Setting the TYPE_ID to 0.0f activates the vertex shader's early-exit gate
                this->m_persistentBufferPtr[i].position = QVector4D (0.0f, 0.0f, 0.0f, 0.0f);
                this->m_persistentBufferPtr[i].velocity = QVector4D (0.0f, 0.0f, 0.0f, 0.0f);
                this->m_persistentBufferPtr[i].metadata = QVector4D (0.0f, 0.0f, 0.0f, 0.0f); // metadata.w() = 0.0f
            }
            
            SIM_LOG (LM_INFO, "VRAM SYSTEM SYNCHRONIZATION: Cleared satellite buffer zone to remove ghost tracks.");
        }
    }
} // namespace SimCore
