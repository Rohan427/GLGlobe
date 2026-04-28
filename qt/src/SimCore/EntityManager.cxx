#include "EntityManager.hxx"
#include "Satellite.hxx"

namespace SimCore
{
    void EntityManager::onDataReceived (const QString& data)
    {
        // Spawn a dedicated, detached thread just for the heavy parsing
        ACE_Thread_Manager::instance()->spawn (
                                               (ACE_THR_FUNC)EntityManager::parsingTask, 
                                               new QString (data), // Pass the data heap-allocated
                                               THR_DETACHED
                                              );
    }
    
    void EntityManager::processTleData (const QString& data)
    {
        QStringList lines = data.split ('\n', Qt::SkipEmptyParts);
        
        // TLEs come in 3-line sets (Name, Line 1, Line 2)
        for (int i = 0; i + 2 < lines.size(); i += 3)
        {
            QString name = lines[i].trimmed();
            std::string l1 = lines[i+1].trimmed().toStdString();
            std::string l2 = lines[i+2].trimmed().toStdString();

            // Safety check for TLE length
            if (l1.length() == 69 && l2.length() == 69)
            {
                auto* sat = new Space::Satellite (name, l1, l2);
                this->addEntity (sat);
            }
        }
    }
}
