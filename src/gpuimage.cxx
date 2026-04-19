#include "sdl-imgui.hxx"

// --- Initialization Stage ---
GLuint image_texture;
glGenTextures (1, &image_texture);
glBindTexture (GL_TEXTURE_2D, image_texture);

// Setup filtering so the image looks crisp when resized
glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

// Assuming your ROCm kernel outputs RGBA8888 (4 bytes per pixel)
// 'width' and 'height' should match your ROCm kernel output size
unsigned char* gpu_output_ptr; // This is the pointer from hipHostMalloc

// --- Inside the Main Loop ---
// 1. Update the texture with the latest ROCm output
glBindTexture (GL_TEXTURE_2D, image_texture);
glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, gpu_output_ptr);

// 2. Render in the "Main Pane"
ImGui::Begin ("Main Viewport");
    // Get the available space in the pane to auto-scale the image
    ImVec2 pane_size = ImGui::GetContentRegionAvail();
    // Display the GPU texture
    ImGui::Image ((void*)(intptr_t)image_texture, pane_size);
ImGui::End();



// --- Initialization ---
unsigned char* host_ptr = nullptr;
size_t img_size = width * height * 4; // Assuming RGBA8888

// Allocate pinned, mapped memory for zero-copy access
hipHostMalloc (&host_ptr, img_size, hipHostMallocMapped);

// --- In the Render Loop ---
// 1. Launch your ROCm kernel
launch_my_kernel (host_ptr, width, height);

// 2. Synchronize to ensure GPU writing is finished before OpenGL reading
hipStreamSynchronize (0);

// 3. Update the OpenGL texture used by the ImGui main pane
glBindTexture (GL_TEXTURE_2D, image_texture);
glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, host_ptr);

