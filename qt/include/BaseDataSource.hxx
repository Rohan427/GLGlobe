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
            virtual void requestUpdate() = 0; // Trigger the download

        signals:
            void dataReceived (const QString& data);
            void errorOccurred (const QString& error);
    };

} // namespace Network
