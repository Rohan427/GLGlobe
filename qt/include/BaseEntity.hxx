#pragma once

#ifndef BASENTITY_HXX
#define BASENTITY_HXX

#ifndef ACE_MT_SAFE
#define ACE_MT_SAFE 1
#endif

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream> // Necessary for the line-by-line reading in fileReaderTask
#include <QDateTime>    // For comparing current time to file age
#include <QObject>
#include "Globe.hxx"
#include <iostream>
#include <cstdlib>
#include <QtMath>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions_4_3_Core>
#include <QVector3D>
#include <QString>
#include <QDateTime>
#include <QThread>
#include <ace/Thread_Mutex.h>
#include "ace/RW_Thread_Mutex.h"
#include <ace/Guard_T.h>
#include <ace/Barrier.h>
#include <ace/Task.h>
#include "ace/Thread.h"
#include <ace/Log_Msg.h>
#include "Utility.hxx"

//#include "MainWindow.hxx"

// SimCore/BaseEntity.hxx
namespace SimCore
{
    static const quint64 MAX_SATELLITES=50000;

    class BaseEntity : public QObject
    {
        Q_OBJECT

        public:
            virtual ~BaseEntity() = default;
            // Every object must be able to update its own 3D position
            virtual void updatePhysics (qint64 msecs, float liveOffset) = 0;
            virtual QVector3D getPosition() const = 0;
            virtual QString getLabel() const = 0;
    };
} // namespace SimCore

#endif // BASENTITY_HXX
