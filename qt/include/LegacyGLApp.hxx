#pragma once

#include <QApplication>
#include <QMouseEvent> // Fixes the "incomplete type" error for mouse
#include <QKeyEvent>   // Fixes it for keyboard
#include <QDebug>      // Required for qDebug()
#include "MyGLWidget.hxx"
#include "MainWindow.hxx"
