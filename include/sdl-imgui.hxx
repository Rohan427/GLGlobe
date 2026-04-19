#pragma once

#include <hip/hip_runtime.h>
#include <hip/hip_gl_interop.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "AppConsole.hxx"
#include <vector>
#include <string>
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include "globe/Globe.hxx"

#define HIP_CHECK(command) { \
    hipError_t status = command; \
        if (status != hipSuccess) { \
        std::cerr << "Error: " << hipGetErrorString(status) << " at line " << __LINE__ << std::endl; \
        exit(EXIT_FAILURE); \
    } \
}

#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#define ERRLNGTH 1000
#define USECONSOLE false

static sdlgl::Console::AppConsole gConsole;
