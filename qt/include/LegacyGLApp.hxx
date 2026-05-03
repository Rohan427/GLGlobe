#pragma once

#ifndef ACE_MT_SAFE
#define ACE_MT_SAFE 1
#endif

#include <QApplication>
#include <QMouseEvent> // Fixes the "incomplete type" error for mouse
#include <QKeyEvent>   // Fixes it for keyboard
#include <QDebug>      // Required for qDebug()
#include <QtMath>
#include "MyGLWidget.hxx"
#include "MainWindow.hxx"
#include <ace/Log_Msg.h>
