#pragma once

#ifndef LEGACYGLAPP_HXX
#define LEGACYGLAPP_HXX

#ifndef ACE_MT_SAFE
#define ACE_MT_SAFE 1
#endif

#include "Globe.hxx"
#include "Config.hxx"
#include "Utility.hxx"
#include <QApplication>
#include <QMouseEvent> // Fixes the "incomplete type" error for mouse
#include <QKeyEvent>   // Fixes it for keyboard
#include <QDebug>      // Required for qDebug()
#include <QtMath>
#include "MyGLWidget.hxx"
#include <ace/Log_Msg.h>


#endif // LEGACYGLAPP_HXX
