/* Use these state variables and event handlers to enable rotation, panning, and zooming.
 */

#include "sdl-imgui.hxx"

float yaw = 0.0f, pitch = 0.0f; 	// Rotation
float zoom = -5.0f;             	// Zoom (Z-distance)
float offsetX = 0.0f, offsetY = 0.0f; 	// Panning
bool isDragging = false;

SDL_Event event;

while (SDL_PollEvent (&event))
{
    ImGui_ImplSDL2_ProcessEvent (&event);

    // Only capture events if the mouse is NOT over an ImGui window
    if (!ImGui::GetIO().WantCaptureMouse)
    {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
        {
            isDragging = true;
        }

        if (event.type == SDL_MOUSEBUTTONUP)
        {
            isDragging = false;
        }

        // Rotation (Left Click + Drag)
        if (event.type == SDL_MOUSEMOTION && isDragging)
        {
            yaw += event.motion.xrel * 0.5f;
            pitch += event.motion.yrel * 0.5f;
        }

        // Zooming (Scroll Wheel)
        if (event.type == SDL_MOUSEWHEEL)
        {
            zoom += event.wheel.y * 0.5f;
        }
    }
}
