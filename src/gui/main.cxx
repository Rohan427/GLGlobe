#include "sdl-imgui.hxx"
#include "LoadShaders.hxx"

sdlgl::LoadShaders::ShaderInfo basicShaders[] =
{
    { GL_VERTEX_SHADER, "shaders/globe.vert" },
    { GL_FRAGMENT_SHADER, "shaders/globe.frag" },
    { GL_NONE, NULL }
};

sdlgl::LoadShaders::ShaderInfo globeShaders[] =
{
    { GL_VERTEX_SHADER, "shaders/globe.vert" },
    { GL_FRAGMENT_SHADER, "shaders/globe.frag" },
    { GL_NONE, NULL }
};

sdlgl::LoadShaders::ShaderInfo earthShaders[] =
{
    { GL_VERTEX_SHADER, "shaders/litGlobe.vert" },
    { GL_FRAGMENT_SHADER, "shaders/earthGlobe.frag" },
    { GL_NONE, NULL }
};

float yaw = 0.0f, pitch = 0.0f; 	// Rotation
float zoom = -5.0f;             	// Zoom (Z-distance)
float offsetX = 0.0f, offsetY = 0.0f; 	// Panning
bool isDragging = false;

GLuint basic;
GLuint planet;
GLuint earth;
SDL_Event event;

GLuint pbo; // OpenGL Pixel Buffer Object
hipGraphicsResource_t hip_res;

// Create a 512x512 checkerboard pattern in memory for testing
std::vector<unsigned char> data (512 * 512 * 4);

void setupInterop (int width, int height)
{
    // 1. Create standard OpenGL Buffer
    glGenBuffers (1, &pbo);
    glBindBuffer (GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData (GL_PIXEL_UNPACK_BUFFER, width * height * 4, NULL, GL_DYNAMIC_DRAW);

    // 2. Register this buffer with HIP
    // This only needs to happen once at startup
    hipGraphicsGLRegisterBuffer (&hip_res, pbo, hipGraphicsRegisterFlagsWriteDiscard);
}

/*
void runKernel (int width, int height)
{
    void* devPtr;
    size_t size;

    // 3. Map the resource (Locks it for HIP use)
    hipGraphicsMapResources (1, &hip_res, 0);
    hipGraphicsResourceGetMappedPointer (&devPtr, &size, hip_res);

    // 4. Launch your AI-generated HIP kernel
    // Pass 'devPtr' as the output buffer
    dim3 block (16, 16);
    dim3 grid (width / 16, height / 16);
    hipLaunchKernelGGL (render_smoothed_earth, grid, block, 0, 0, (float4*)devPtr, width, height);

    // 5. Unmap (Hand it back to OpenGL for rendering)
    hipGraphicsUnmapResources (1, &hip_res, 0);
}
*/

GLuint CreateSimpleTexture (int width, int height, SDL_GLContext gl_context, SDL_Window* window)
{
    GLuint textureID, realID, dummyID;

    SDL_GL_MakeCurrent (window, gl_context); // Force sync right before Gen
    glGenTextures (1, &dummyID);
    glGenTextures (1, &realID);
    textureID = realID;

//    printf ("Current Context: %p\n", SDL_GL_GetCurrentContext());
    printf ("Assigned Texture ID: %u\n", textureID);

    glBindTexture (GL_TEXTURE_2D, textureID);

    // 1. MUST set alignment for AMD/Mesa drivers
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

    // Create a 512x512 checkerboard pattern in memory
 //   std::vector<unsigned char> data (width * height * 4);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++) 
        {
            int index = (y * width + x) * 4;
            // Simple checkerboard logic
            unsigned char color = ((x / 32 + y / 32) % 2 == 0) ? 255 : 100;
            data[index + 0] = color;       // R
  //          data[index + 1] = 0;           // G
            data[index + 2] = 255 - color; // B
            data[index + 3] = 255;         // A
            data[index + 1] = 255;
        }
    }

    // 2. Use GL_RGBA8 for the internal format (explicit size)
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

    // 3. Mandatory Filters - Without these, some AMD drivers fail to bind
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Check for errors
    GLenum err = glGetError();

    if (err != GL_NO_ERROR)
    {
        gConsole.addLog ("OpenGL Error during texture creation: %04x\n", err);
    }
    
//    glBindTexture (GL_TEXTURE_2D, 0);

    // 4. Force a flush to ensure the GPU actually gets the data
    glFlush();

    std::cout << "New texture ID: " << textureID << std::endl;

    return textureID;
}

void BindMyTextureCallback (const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    GLuint texID = (GLuint)(intptr_t)cmd->UserCallbackData;
    glBindTexture (GL_TEXTURE_2D, texID);
}

void BindTextureCallback (const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    // Force bind the actual texture ID passed in UserCallbackData
    GLuint texID = (GLuint)(intptr_t)cmd->UserCallbackData;
    glBindTexture (GL_TEXTURE_2D, texID);
}


int main (int argc, char* argv[])
{
    gConsole.addLog ("Application Starting...\n");

    GLuint image_texture;
    static float globalFontScale = 1.0f;
    sdlgl::LoadShaders shaderLoader;

    // 1. Setup SDL
    gConsole.addLog ("Initialize SDL...\n");

    SDL_Init (SDL_INIT_VIDEO);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_Window* window = SDL_CreateWindow ("sdl-imgui Test",
                                            1280,
                                            720,
                                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
                                          );
    SDL_GLContext gl_context = SDL_GL_CreateContext (window);
    SDL_GL_MakeCurrent (window, gl_context);

    // --- GL Initialization Stage ---
    GLenum err = glewInit();

    if (GLEW_OK != err)
    {
        /* Problem: glewInit failed, something is seriously wrong. */
        fprintf (stderr, "Error: %s\n", glewGetErrorString (err));
    }

    gConsole.addLog ("Initialize OpenGL...\n");

    // Assuming your ROCm kernel outputs RGBA8888 (4 bytes per pixel)
    // 'width' and 'height' should match your ROCm kernel output size
    unsigned char* gpu_output_ptr; // This is the pointer from hipHostMalloc

    // 2. Setup Dear ImGui
    gConsole.addLog ("Initialize imgui...\n");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    // 1. Load a system font (e.g., DejaVuSans which is standard on RHEL)
    // The second parameter is the font size in pixels (e.g., 24.0f or 32.0f)
    ImFont* font = io.Fonts->AddFontFromFileTTF ("/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf", 24.0f);

    if (font == nullptr)
    {
        // Fallback: If the specific path fails, ImGui will use the default tiny font
        fprintf (stderr, "Could not load font! Check the path.\n");
        gConsole.addLog ("Could not load font! Check the path.\n");
    }
    // else continue with default font

    // 2. Scale the entire UI style to match the larger font
    ImGui::GetStyle().ScaleAllSizes (2.0f); // Scales checkboxes, spacing, and title bars

    ImGui_ImplSDL3_InitForOpenGL (window, gl_context);
    ImGui_ImplOpenGL3_Init(); // "#version 130"
    image_texture = CreateSimpleTexture (512, 512, gl_context, window);
/*
    if ((basic = shaderLoader.load (basicShaders, &gConsole)) == 0)
    {
        std::cerr << "Shader program basic not initialized" << std::endl;

        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext (gl_context);
        SDL_DestroyWindow (window);
        SDL_Quit();

        return 1;
    }

    if ((planet = shaderLoader.load (globeShaders, &gConsole)) == 0)
    {
        std::cerr << "Shader program Globe not initialized" << std::endl;

        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext (gl_context);
        SDL_DestroyWindow (window);
        SDL_Quit();

        return 1;
    }
*/
/*
    if ((earth = shaderLoader.load (earthShaders, &gConsole)) == 0)
    {
        std::cerr << "Shader program Earth not initialized" << std::endl;

        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext (gl_context);
        SDL_DestroyWindow (window);
        SDL_Quit();

        return 1;
    }
*/

    bool done = false;
    static ImGuiStyle styleBackup = ImGui::GetStyle(); // Run once at startup

    gConsole.addLog ("Application Started...\n");

    while (!done)
    {
        SDL_Event event;

        while (SDL_PollEvent (&event))
        {
            ImGui_ImplSDL3_ProcessEvent (&event);

            if (event.type == SDL_EVENT_QUIT)
            {
                done = true;
            }
            // Only capture events if the mouse is NOT over an ImGui window
            else if (!ImGui::GetIO().WantCaptureMouse)
            {
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT)
                {
                    isDragging = true;
                }

                if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
                {
                    isDragging = false;
                }

                // Rotation (Left Click + Drag)
                if (event.type == SDL_EVENT_MOUSE_MOTION && isDragging)
                {
                    yaw += event.motion.xrel * 0.5f;
                    pitch += event.motion.yrel * 0.5f;
                }

                // Zooming (Scroll Wheel)
                if (event.type == SDL_EVENT_MOUSE_WHEEL)
                {
                    zoom += event.wheel.y * 0.5f;
                }
            }
        }

        // Start ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // 3. Menu Bar
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu ("File"))
            {
                if (ImGui::MenuItem ("Exit"))
                {
                    done = true;
                }

                ImGui::EndMenu();
            }

            // 2. Inside your ImGui Rendering Loop (e.g., in a Window or Menu)
            if (ImGui::BeginMenu ("Settings"))
            {
                // Slider to adjust font scale from 1.0x to 3.0x
                if (ImGui::SliderFloat ("Global Font Scale", &globalFontScale, 1.0f, 3.0f, "%.1f"))
                {
                    // Apply the scale to the IO structure
                    ImGui::GetIO().FontGlobalScale = globalFontScale;

                    // Reset and rescale the entire interface style
                    ImGui::GetStyle() = styleBackup;
                    ImGui::GetStyle().ScaleAllSizes(globalFontScale);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        // 4. Main Pane (GPU Images)
        ImGui::SetNextWindowPos (ImVec2 (0, 30)); // Below menu
        ImGui::SetNextWindowSize (ImVec2 (ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y * 0.7f));

        // 1. Force the texture to be valid right before ImGui starts its frame
//        glBindTexture (GL_TEXTURE_2D, image_texture); 
        // (Optional) Re-verify parameters 
//        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//        glBindTexture (GL_TEXTURE_2D, 0);

        ImGui::Begin ("Main Viewport", nullptr, ImGuiWindowFlags_NoDecoration);

        // 1. Force the driver to unbind EVERYTHING before we draw
        // This helps break the "stuck" font atlas binding in Mesa 25
        ImGui::GetWindowDrawList()->AddCallback ([](const ImDrawList*, const ImDrawCmd*)
        {
            glBindTexture (GL_TEXTURE_2D, 0); 
            glUseProgram (0);
        }, nullptr);

        // 2. Draw a dummy spacer to force a new draw command
        ImGui::Dummy (ImVec2 (512, 512));

        // 3. Manually bind and draw the image using the draw list
        ImVec2 p_min = ImGui::GetItemRectMin();
        ImVec2 p_max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddImage ((ImTextureID)(intptr_t)image_texture, p_min, p_max);



        // 1. FORCE the context to be active for THIS window
/**        SDL_GL_MakeCurrent (SDL_GL_GetCurrentWindow(), SDL_GL_GetCurrentContext());

        // 2. RE-UPLOAD the pixels to ID 2 right now
        glBindTexture (GL_TEXTURE_2D, (GLuint)2);
        // Use a different color (like pure Green) to see if it changes
        static std::vector<unsigned char> greenData (512 * 512 * 4, 0);

        for (int i=0; i<greenData.size(); i+=4)
        {
            greenData[i+1] = 255;
            greenData[i+3] = 255;
        }

        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, greenData.data());
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // 3. Draw
        ImGui::Image ((ImTextureID)(intptr_t)2, ImVec2 (512, 512));
**/

/*
        ImVec2 size = ImVec2 (512, 512);

        // A. Insert callback to FORCE bind your checkerboard (ID 2)
        ImGui::GetWindowDrawList()->AddCallback(BindTextureCallback, (void*)(intptr_t)image_texture);

        // DEBUG: Print the ID to console once to make sure it's not 0
        static bool firstFrame = true;

        if (firstFrame)
        {
            printf ("Using Texture ID: %u\n", (unsigned int)image_texture);
            firstFrame = false;
        }

        // printf ("Texture ID: %d\n", image_texture);
        ImGui::Image ((ImTextureID)(intptr_t)image_texture, size);

        ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
*/

        ImGui::End();

        // 5. Output/Error Pane
        if (USECONSOLE)
        {
            ImGui::SetNextWindowPos (ImVec2 (0, ImGui::GetIO().DisplaySize.y * 0.72f));
            ImGui::SetNextWindowSize (ImVec2 (ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y * 0.25f));

            gConsole.draw ("Output Console");
        }

        // Rendering
        ImGui::Render();
//        SDL_GL_MakeCurrent (window, gl_context); 
        glViewport (0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        glClear (GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData (ImGui::GetDrawData());
        SDL_GL_SwapWindow (window);
    } // END-WHILE: while (!done)

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow (window);
    SDL_Quit();

    return 0;
}

