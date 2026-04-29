#include "CelesTrakSource.hxx"
#include "MainWindow.hxx"

namespace Network
{
    void CelesTrakSource::requestUpdate()
    {
        // 1. Create a human-readable timestamp (e.g., satellites_20260428_1637.tle)
        QString timestamp = QDateTime::currentDateTime().toString ("yyyyMMdd_hhmm");
        QString fileName = QString ("%1/satellites_%2_%3").arg (Globe::DATA_DIR_PATH)
                                                          .arg (timestamp)
                                                          .arg (Globe::DATA_FILE_SAT_SUFFIX);

        // Current CelesTrak GP API for active stations in TLE format
        QUrl url ("https://celestrak.org/NORAD/elements/gp.php?GROUP=STARLINK&FORMAT=TLE");
        
        QNetworkRequest request (url);

        // Identifies your app and requests raw text
        request.setHeader (QNetworkRequest::UserAgentHeader, "LegacyGLGlobe/1.0");
        request.setRawHeader ("Accept", "text/plain");

        // Qt 6: Follow redirects but keep it secure
        request.setAttribute (QNetworkRequest::RedirectPolicyAttribute, 
                         QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply* reply = m_manager.get (request);

        // Create or overwrite the local cache file
        QFile* cacheFile = new QFile (fileName);

        if (!cacheFile->open (QIODevice::WriteOnly | QIODevice::Text))
        {
            delete cacheFile;
            return;
        }

        // STREAMING: Write to disk as data arrives
        connect (reply, &QNetworkReply::readyRead, [reply, cacheFile]()
        {
            cacheFile->write (reply->readAll());
            cacheFile->flush(); // Ensure it's physically on the disk
        });

        connect(reply, &QNetworkReply::finished, [this, reply, cacheFile]()
        {
            cacheFile->close();
            delete cacheFile;
            if (reply->error() == QNetworkReply::NoError)
            {
                // Now notify the manager to read the completed file
                emit dataReceived ("FILE_READY:satellites_cache.tle");
            }
            reply->deleteLater();
        });
    }
} // namespace Network
