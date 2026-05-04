#pragma once

#ifndef CELESTRAKSOURCE_HXX
#define CELESTRAKSOURCE_HXX

#include "Utility.hxx"
#include "Globe.hxx"
#include "BaseDataSource.hxx"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QFile>

namespace Network
{
    class CelesTrakSource : public BaseDataSource
    {
        private:
            Q_OBJECT
            
            QNetworkAccessManager m_manager;

            // Add a group member
            QString m_group;

            const QMap<QString, QString> m_tleGroups =
            {
                {"STARLINK", "Starlink Constellation"},
                {"STATIONS", "Space Stations"},
                {"ORBCOMM", "Orbcomm"},
                {"GPS-OPS", "GPS Operational"},
                {"ACTIVE", "All Active Satellites"},
                {"VISUAL", "Brightest Satellites"}
            };

        public:
//            void requestUpdate() override;
            void initiateDownload (const QUrl& url, const QString& localPath, const QString& groupKey) override;
            void requestGroup (const QString& groupKey = "STARLINK") override;

            QMap<QString, QString> getGroups() const
            {
                return m_tleGroups;
            }

            QString getGroup() const
            {
                return m_group;
            }

            void setGroup (QString group)
            {
                m_group = group;
            }
    };
} // namespace Network

#endif // CELESTRAKSOURCE_HXX
