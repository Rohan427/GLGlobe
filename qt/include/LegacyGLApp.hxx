#pragma once

#include <QApplication>
#include <QMouseEvent> // Fixes the "incomplete type" error for mouse
#include <QKeyEvent>   // Fixes it for keyboard
#include <QDebug>      // Required for qDebug()
#include <QtMath>
#include "MyGLWidget.hxx"
#include "MainWindow.hxx"
#include <ace/Log_Msg.h>
