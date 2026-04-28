#pragma once

#include "BaseDataSource.hxx"
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace Network
{
    class CelesTrakSource : public BaseDataSource
    {
        private:
            Q_OBJECT
            
            QNetworkAccessManager m_manager;

        public:
            void requestUpdate() override
            {
                // Fetches all 'active' stations in TLE format
                QUrl url("https://celestrak.org");
                
                QNetworkRequest request (url);
                QNetworkReply* reply = m_manager.get(request);

                connect(reply, &QNetworkReply::finished, [this, reply]()
                {
                    if (reply->error() == QNetworkReply::NoError)
                    {
                        emit dataReceived (QString::fromUtf8 (reply->readAll()));
                    }
                    else
                    {
                        emit errorOccurred (reply->errorString());
                    }

                    reply->deleteLater();
                });
            }
    };
} // namespace Network
