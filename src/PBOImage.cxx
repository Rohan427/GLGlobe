#include "sdl-imgui.hxx"

// --- Initialization ---
GLuint pbo;
glGenBuffers (1, &pbo);
glBindBuffer (GL_PIXEL_UNPACK_BUFFER, pbo);

// Allocate PBO storage (NULL means we'll provide data later)
glBufferData (GL_PIXEL_UNPACK_BUFFER, img_size, NULL, GL_STREAM_DRAW);
glBindBuffer (GL_PIXEL_UNPACK_BUFFER, 0);

// --- Inside the Main Render Loop ---
// 1. Ensure ROCm kernel is done
hipStreamSynchronize (0); 

// 2. Start the Asynchronous Transfer
glBindBuffer (GL_PIXEL_UNPACK_BUFFER, pbo);

// Map the PBO memory for a fast CPU-to-GPU copy (or use glBufferSubData)
void* pbo_ptr = glMapBufferRange (GL_PIXEL_UNPACK_BUFFER, 0, img_size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
memcpy (pbo_ptr, host_ptr, img_size); // host_ptr is your ROCm output
glUnmapBuffer (GL_PIXEL_UNPACK_BUFFER);

// 3. Update Texture from PBO (This call returns immediately)
glBindTexture (GL_TEXTURE_2D, image_texture);
glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0); // '0' offset means use PBO
glBindBuffer (GL_PIXEL_UNPACK_BUFFER, 0);

// 4. Render the texture in the ImGui pane as before
ImGui::Image ((void*)(intptr_t)image_texture, pane_size);

// Log buffer for the Output Pane
if (hip_error_occurred)
{
    gConsole.addLog ("ROCm Error: " + std::string (hipGetErrorString (err)));
}

ImGui::End();
