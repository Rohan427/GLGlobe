#pragma once

#include "sdl-imgui.hxx"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

namespace sdlgl
{
    class Console
    {
        public:
            struct AppConsole
            {
                ImGuiTextBuffer Buf;
                bool ScrollToBottom = true;

                void clear()
                {
                    Buf.clear();
                }

                void addLog (const char* fmt, ...)
                {
#if USECONSOLE
                    {
                        va_list args;
                        va_start (args, fmt);
                        Buf.appendfv (fmt, args);
                        va_end (args);
                        ScrollToBottom = true; // Trigger auto-scroll on new entry
                    }
#else
                    {
                        va_list args;
                        va_start (args, fmt);
                        vprintf (fmt, args);
                    }
#endif
                }

                void draw (const char* title, bool* p_open = NULL)
                {
                    ImGui::Begin (title, p_open);

                    if (ImGui::Button ("Clear"))
                    {
                        clear();
                    }

                    ImGui::Separator();

                    // Use a child window for the scrollable region
                    ImGui::BeginChild ("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                    
                    // Output the text buffer
                    ImGui::TextUnformatted (Buf.begin());

                    // Automatic scrolling logic
                    if (ScrollToBottom)
                    {
                        ImGui::SetScrollHereY (1.0f); // Scroll to the very bottom
                    }

                    ScrollToBottom = false;

                    ImGui::EndChild();
                    ImGui::End();
                }
            };
    };
}
