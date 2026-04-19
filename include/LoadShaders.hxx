#pragma once

#include "sdl-imgui.hxx"
#include <iostream>
#include <fstream>
#include <memory>

namespace sdlgl
{
	class LoadShaders
	{
        public:
            // Structure to pass shader files to the function
            typedef struct
            {
                GLenum       type;
                const char*  filename;
                GLuint       shader;
            } ShaderInfo;

            GLuint load (ShaderInfo* shaders, sdlgl::Console::AppConsole* logs);
            std::string ReadShader (const char* filename);
	};
}
