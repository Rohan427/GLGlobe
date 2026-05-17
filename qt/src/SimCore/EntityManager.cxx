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
 //           std::cout << "using file reader task" << std::endl;

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

 //           std::cout << "using parsing task" << std::endl;

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

//        std::cout << "Total Line: " << lines.size() << std::endl;
        
        std::vector<BaseEntity*> newSats;
        int parsedCount = 0;

        // TLEs are 3-line blocks. We must ensure we have a full triplet.
        for (int i = 0; i + 2 < lines.size();)
        {
            QString name = lines[i].trimmed();
            QString l1 = lines[i+1];
            QString l2 = lines[i+2];

//            std::cout << "Name: " << name.toStdString() << ", L1: " << l1.toStdString() << ", L2: " << l2.toStdString() << std::endl;

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
        else
        {
//            std::cout << "EntityManager::parsingTask: New EntityManager is null" << std::endl;
        }

//        SIM_LOG (LM_INFO, QString ("SUCCESS: Parsed %1 %2").arg (parsedCount).arg ("satellites."));

        return nullptr;
    }

    void EntityManager::addBatch (const std::vector<BaseEntity*>&& newEntities)
    {
//        std::cout << "EntityManager::addBatch: Adding batch of " << newEntities.size() << " entities" << std::endl;

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
        } // ACE_GUARD (ACE_Thread_Mutex, mon, m_vectorLock);

//        std::cout << "EntityManager::addBatch complete" << std::endl;
    }

    void* EntityManager::fileReaderTask (void* arg)
    {
        // 1. Capture and wrap in a smart pointer immediately for safety
        std::unique_ptr<FileTaskData> data (static_cast<FileTaskData*> (arg));

//        std::cout << "EntityManager::fileReaderTask: Reading file " << data->path.toStdString() << std::endl;

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

//        std::cout << "Leaving EntityManager::fileReaderTask\n\n\n\n" << std::endl;
        return nullptr; // data (unique_ptr) is deleted here automatically
    }

    void EntityManager::startSimulation (int numThreads)
    {
        m_tracker = new Objects::Tracking();
        s_instance = this;
        m_numThreads = numThreads;

        // 32 workers + 1 main thread = 33
        m_barrier = new ACE_Barrier (m_numThreads + 1); 
        this->activate (THR_NEW_LWP | THR_JOINABLE | THR_BOUND, m_numThreads);

        // This blocks the MAIN thread until all numThreads workers hit their own wait()
        // It is a very fast "handshake," not a long-term freeze.
        m_barrier->wait();

        SIM_LOG (LM_DEBUG, QString ("All %1 ACE threads synchronized and running.").arg (m_numThreads));
    }

    void EntityManager::removeByGroup (const QString& groupKey)
    {
        std::vector<BaseEntity*> toDelete;

//        ACE_DEBUG((LM_INFO, ACE_TEXT("[TID:%t] Removing group: %s\n"), groupKey.toUtf8().constData()));
        std::cout << "removing group " << groupKey.toUtf8().constData() << std::endl;
        
        {
            ACE_GUARD (ACE_Thread_Mutex, mon, EntityManager::m_vectorLock);
            
//            std::cout << "ACE_GUARD lock" << std::endl;

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
        }

 //       std::cout << "ACE_GUARD released" << std::endl;

  //      std::cout << "free memory" << std::endl;

        // Free memory
        for (auto* e : toDelete)
        {
            if (!e) continue;
 //           std::cout << "delete e" << std::endl;
            delete e; 
        }

 //        std::cout << "free completed" << std::endl;
        

        SIM_LOG (LM_INFO, QString ("Removed group %1. Current count: %2").arg (groupKey).arg (m_entities.size()));
    }
}
