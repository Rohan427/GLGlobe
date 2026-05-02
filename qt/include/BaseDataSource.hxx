#pragma once

#include <QObject>
#include <QString>

namespace Network
{
    class BaseDataSource : public QObject
    {
        private:
            Q_OBJECT

        public:
            virtual ~BaseDataSource() = default;
//            virtual void requestUpdate() = 0; // Trigger the download

            // Handle dynamic group requests
            virtual void requestGroup (const QString& groupKey) = 0;

        protected:
            // Internal helper for streaming specific URLs to disk
            virtual void initiateDownload (const QUrl& url, const QString& localPath, const QString& groupKey) = 0;

        signals:
            void dataReceived (const QString& data, const QString& group);
            void errorOccurred (const QString& error);
    };

} // namespace Network
