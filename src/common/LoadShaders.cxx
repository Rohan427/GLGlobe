#include "LoadShaders.hxx"

using namespace sdlgl;

// Function to read shader files
std::string LoadShaders::ReadShader (const char* filename)
{
    std::cerr << "Reading file " << filename << std::endl;

    std::ifstream file (filename, std::ios::in);

    if (!file.is_open())
    {
        std::cerr << "Failed to read file: " << filename << std::endl;
        return "";
    }

    std::string content ((std::istreambuf_iterator<char> (file)),
                          std::istreambuf_iterator<char>());
    file.close();

    return content;
}


// The LoadShaders function
GLuint LoadShaders::load (ShaderInfo* shaders, sdlgl::Console::AppConsole* logs)
{
    std::cout << "Loading shaders..." << std::endl;
    logs->addLog ("Loading shaders...\n");
    auto infoLog = std::make_unique<char[]>(ERRLNGTH);
    char* infoPtr = infoLog.get();

    GLint buildResult = GL_TRUE;

    if (shaders == NULL)
    {
        std::cerr << "ERROR: No shaders provided" << std::endl;
        logs->addLog ("ERROR: No shaders provided\n");

	    return 0;
    }

    std::cout << "Create program..." << std::endl;
    logs->addLog ("Create program...\n");

    GLuint program = glCreateProgram();
    ShaderInfo* entry = shaders;

    if (program > 0)
    {
        while ((entry->type != GL_NONE) && buildResult)
        {
            std::string source = ReadShader (entry->filename);
            logs->addLog ("Shader: %s\n", entry->filename);

            if (source.empty())
            {
                std::string error (std::string ("ERROR: ") + source.c_str() + " empty\n");
                logs->addLog (error.c_str());

                return 0;
            }

            GLuint shader = glCreateShader (entry->type);
            entry->shader = shader;
            const char* srcPtr = source.c_str();
            glShaderSource (shader, 1, &srcPtr, NULL);
            glCompileShader (shader);

            glGetShaderiv (shader, GL_COMPILE_STATUS, &buildResult);
            
            if (!buildResult)
            {
                GLsizei length;
                glGetShaderInfoLog (shader, ERRLNGTH, &length, infoPtr);
                logs->addLog (infoPtr);
            }
            else
            {
                glAttachShader (program, shader);
                entry++;
            }
        }

        if (buildResult)
        {
            glLinkProgram (program);
            glGetProgramiv (program, GL_LINK_STATUS, &buildResult);
            
            if (!buildResult)
            {
                GLsizei length;
                glGetProgramInfoLog (program, ERRLNGTH, &length, infoPtr);
                logs->addLog (infoPtr);
            }
        }
        else
        {
            glDeleteProgram (program);
        }
    }
    else
    {
        logs->addLog ("FATAL ERROR: Failed to create shader program");
    }

    return program;
}

