#include "EntityManager.hxx"
#include "Satellite.hxx"
#include "MainWindow.hxx"

namespace SimCore
{
    ACE_Thread_Mutex EntityManager::m_vectorLock;

    EntityManager* EntityManager::s_instance = nullptr;

    EntityManager* EntityManager::instance()
    {
        return s_instance;
    }

    void EntityManager::onDataReceived (const QString& data)
    {
        // Spawn a dedicated, detached thread just for the heavy parsing
        ACE_Thread_Manager::instance()->spawn (
                                               (ACE_THR_FUNC)EntityManager::parsingTask, 
                                               new QString (data), // Pass the data heap-allocated
                                               THR_DETACHED
                                              );
    }
    
    void EntityManager::processTleData (const QString& info)
    {
        std::cout << "Parsing TLE data..." << std::endl;
        
        if (info.startsWith ("FILE_READY:"))
        {
            QString fileName = info.mid (11);
            
            // Use a dedicated ACE task to read the file
            ACE_Thread_Manager::instance()->spawn ((ACE_THR_FUNC) EntityManager::fileReaderTask, 
                                                   new QString (fileName), 
                                                   THR_DETACHED
                                                  );
        }
    }

    // Static helper for the dedicated parsing thread
    void* EntityManager::parsingTask (void* arg)
    {
        if (MainWindow::instance())
        {
            MainWindow::instance()->logMessage ("Starting parsing task...");
        }

        std::cout << "Starting parsing task..." << std::endl;

        QString* rawData = static_cast<QString*> (arg);
        
        // Split by any newline variation (\r\n, \n, \r)
        QStringList lines = rawData->split (QRegularExpression ("(\r\n|\n|\r)"), Qt::SkipEmptyParts);
        
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
                    auto* sat = new Space::Satellite (name, line1, line2);
                    newSats.push_back (sat);
                    parsedCount++;
                }
                catch (...)
                {
                    if (MainWindow::instance())
                    {
                        MainWindow::instance()->logMessage ("Skipping malformed satellite.");
                    }

                    std::cout << "Skipping malformed satellite." << std::endl;
                }

                i += 3; // Move to next triplet
            }
            else
            {
                // We are out of sync! Move forward one line at a time until we find a '1 '
                i++; 
            }
        }

        if (EntityManager::instance())
        {
            EntityManager::instance()->addBatch (newSats);
        }

        std::cout << "SUCCESS: Parsed " << parsedCount << " satellites." << std::endl;

        if (MainWindow::instance())
        {
            MainWindow::instance()->logMessage (QString ("SUCCESS: Parsed %1 %2").arg (parsedCount).arg ("satellites."));
        }

        delete rawData;

        return nullptr;
    }

    void EntityManager::addBatch (const std::vector<BaseEntity*>& newEntities)
    {

        SIM_LOG ("Adding batch");

        if (newEntities.empty())
        {
            SIM_LOG ("No new entities");

            return;
        }
        else
        {
            SIM_LOG ((QString ("Entities to parse: %1").arg (newEntities.size())));
        }

        SIM_LOG ("Lock vector and load it");
        {
            // Lock the vector once for the whole batch
            ACE_GUARD (ACE_Thread_Mutex, mon, m_vectorLock);
            
            SIM_LOG ("Reserve entity memory");
            // Use reserve to prevent multiple reallocations
            m_entities.reserve (m_entities.size() + newEntities.size());

            SIM_LOG ("Copy entity data");
            // Manual copy to catch bad pointers
            for (auto* entity : newEntities)
            {
                if (entity)
                {
                    m_entities.push_back (entity);
                }
            }
        } // ACE_GUARD (ACE_Thread_Mutex, mon, m_vectorLock);

        SIM_LOG ((QString ("Batch added: %1 new objects registered.").arg (newEntities.size())));
    }

    void* EntityManager::fileReaderTask (void* arg)
    {
        QString* fileName = static_cast<QString*> (arg);

        if (MainWindow::instance())
        {
            MainWindow::instance()->logMessage (QString ("reading file %1").arg (fileName->toStdString()));
        }

        std::cout << QString ("Reading file %1").arg (fileName->toStdString()).toStdString() << std::endl;

        QFile file (*fileName);

        if (MainWindow::instance())
        {
            MainWindow::instance()->logMessage (QString ("Opening file %1").arg (fileName->toStdString()));
        }

        std::cout << QString ("Opening file %1").arg (fileName->toStdString()).toStdString() << std::endl;
        
        if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
        {
            delete fileName;

            return nullptr;
        }

        QTextStream in (&file);
        std::vector<BaseEntity*> batch;
        
        // Local buffers to avoid excessive allocations
        QString name, l1, l2;
        
        if (MainWindow::instance())
        {
            MainWindow::instance()->logMessage (QString ("Parsing file %1 TLEs").arg (fileName->toStdString()));
        }

        std::cout << QString ("Parsing file %1 TLEs").arg (fileName->toStdString()).toStdString() << std::endl;

        while (!in.atEnd())
        {
            name = in.readLine().trimmed();
            l1 = in.readLine();
            l2 = in.readLine();

            if (l1.startsWith ("1 ") && l2.startsWith ("2 "))
            {
                // Padding logic to ensure libsgp4 doesn't throw TleException
                std::string line1 = l1.leftJustified (69, ' ').left (69).toStdString();
                std::string line2 = l2.leftJustified (69, ' ').left (69).toStdString();

                try
                {
                    batch.push_back (new Space::Satellite (name, line1, line2));
                }
                catch (...)
                {
                    if (MainWindow::instance())
                    {
                        MainWindow::instance()->logMessage ("Skipping bad TLEs.");
                    }

                    std::cout << "Skipping bad TLEs." << std::endl;
                }
            }
            
            // Performance: Every 1000 sats, drop them into the manager 
            // so the globe starts populating while we read the rest.
            if (batch.size() >= 1000)
            {
                EntityManager::instance()->addBatch (batch);
                batch.clear();
            }
        }

        // Add any remaining sats
        EntityManager::instance()->addBatch (batch);

        file.close();
        delete fileName;

        return nullptr;
    }

    void EntityManager::startSimulation (int numThreads)
    {
        s_instance = this;

        m_numThreads = numThreads;

        // 32 workers + 1 main thread = 33
        m_barrier = new ACE_Barrier (m_numThreads + 1); 
        this->activate (THR_NEW_LWP | THR_JOINABLE, m_numThreads);

        // This blocks the MAIN thread until all numThreads workers hit their own wait()
        // It is a very fast "handshake," not a long-term freeze.
        m_barrier->wait();


        SIM_LOG (QString ("All %1 ACE threads synchronized and running.").arg (m_numThreads));
        
    }
}
