#pragma once

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
#include <ace/Guard_T.h>
#include "Globe.hxx"
//#include "MainWindow.hxx"

// SimCore/BaseEntity.hxx
namespace SimCore
{
    static const quint64 MAX_SATELLITES=50000;

    class BaseEntity
    {
        public:
            virtual ~BaseEntity() = default;
            // Every object must be able to update its own 3D position
            virtual void updatePhysics (qint64 msecs, float liveOffset) = 0;
            virtual QVector3D getPosition() const = 0;
            virtual QString getLabel() const = 0;
    };
} // namespace SimCore
