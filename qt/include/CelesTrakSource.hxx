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
            void requestUpdate() override;
    };
} // namespace Network
