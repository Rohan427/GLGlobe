#include "CelesTrakSource.hxx"
#include "MainWindow.hxx"

namespace Network
{
    //void CelesTrakSource::requestUpdate()
    //{
    //    // 1. Create a human-readable timestamp (e.g., satellites_20260428_1637.tle)
    //    QString timestamp = QDateTime::currentDateTime().toString ("yyyyMMdd_hhmm");
    //    QString fileName = QString ("%1/satellites_%2_%3").arg (Globe::DATA_DIR_PATH)
    //                                                      .arg (timestamp)
    //                                                      .arg (Globe::DATA_FILE_SAT_SUFFIX);

    //    // Current CelesTrak GP API for active stations in TLE format
    //    QUrl url ("https://celestrak.org/NORAD/elements/gp.php?GROUP=STARLINK&FORMAT=TLE");
    //    
    //    QNetworkRequest request (url);

    //    // Identifies your app and requests raw text
    //    request.setHeader (QNetworkRequest::UserAgentHeader, "LegacyGLGlobe/1.0");
    //    request.setRawHeader ("Accept", "text/plain");

    //    // Qt 6: Follow redirects but keep it secure
    //    request.setAttribute (QNetworkRequest::RedirectPolicyAttribute, 
    //                          QNetworkRequest::NoLessSafeRedirectPolicy);

    //    QNetworkReply* reply = m_manager.get (request);

    //    // Create or overwrite the local cache file
    //    QFile* cacheFile = new QFile (fileName);

    //    if (!cacheFile->open (QIODevice::WriteOnly | QIODevice::Text))
    //    {
    //        delete cacheFile;
    //        return;
    //    }

    //    // STREAMING: Write to disk as data arrives
    //    connect (reply, &QNetworkReply::readyRead, [reply, cacheFile]()
    //    {
    //        cacheFile->write (reply->readAll());
    //        cacheFile->flush(); // Ensure it's physically on the disk
    //    });

    //    connect(reply, &QNetworkReply::finished, [this, reply, cacheFile]()
    //    {
    //        cacheFile->close();
    //        delete cacheFile;

    //        if (reply->error() == QNetworkReply::NoError)
    //        {
    //            // Now notify the manager to read the completed file
    //            emit dataReceived ("FILE_READY:" + fileName.toStdString(), groupKey);
    //        }

    //        reply->deleteLater();
    //    });
    //}

    void CelesTrakSource::requestGroup (const QString& groupKey)
    {
//        std::cout << "CelesTrakSource::requestGroup: Requesting group " << groupKey.toStdString() << std::endl;

        // 1. Build the path using the group key and current date/time
        QString fileName = QString ("data/satellites_%1_%2.tle")
                           .arg (groupKey.toLower())
                           .arg (QDateTime::currentDateTime().toString ("yyyyMMdd_hhmm"));

        // 2. Build the dynamic CelesTrak URL
        QString baseUrl = Globe::CELESTRAK_URL;
        QString query = QString ("GROUP=%1&FORMAT=tle").arg (groupKey.toLower());
        QUrl url (baseUrl + query);

//        std::cout << "CelesTrakSource::requestGroup: URL is: " << url.toString().toUtf8().constData() << std::endl;

        // 3. Kick off the download
        this->initiateDownload (url, fileName, groupKey);
    }

    void CelesTrakSource::initiateDownload(const QUrl& url, const QString& localPath, const QString& groupKey)
    {
//        std::cout << "CelesTrakSource::initiateDownload: Initiating download " << groupKey.toStdString() << std::endl;

        QNetworkRequest request (url);
        request.setHeader (QNetworkRequest::UserAgentHeader, "LegacyGLGlobe/1.0");
        request.setRawHeader ("Accept", "text/plain");
        request.setAttribute (QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply* reply = m_manager.get (request);

        // 1. Create the file on the heap so the lambdas can access it
        QFile* cacheFile = new QFile (localPath);

        if (!cacheFile->open (QIODevice::WriteOnly | QIODevice::Text))
        {
            ACE_ERROR ((LM_ERROR, ACE_TEXT ("Failed to open cache file: %s\n"), localPath.toUtf8().constData()));
            delete cacheFile;
            reply->abort();
            return;
        }

        // 2. STREAMING: Write chunks to disk as they arrive over the network
        connect (reply, &QNetworkReply::readyRead, [reply, cacheFile]()
        {
            cacheFile->write (reply->readAll());
            cacheFile->flush(); // Important for the "Slow Human" to see progress on disk
        });

        // 3. FINISHED: Close up and notify the Manager
        connect (reply, &QNetworkReply::finished, [this, reply, cacheFile, groupKey, localPath]()
        {
            cacheFile->close();
            
            if (reply->error() == QNetworkReply::NoError)
            {
                ACE_ERROR ((LM_ERROR, ACE_TEXT ("Download Complete")));
                emit dataReceived ("FILE_READY:" + localPath, groupKey);
            }
            else
            {
                ACE_ERROR ((LM_ERROR, ACE_TEXT ("Download failed")));
                cacheFile->remove(); // Clean up the empty/broken file
            }
            
            delete cacheFile; // Safe to delete now that we're finished
            reply->deleteLater();
        });
    }
} // namespace Network
