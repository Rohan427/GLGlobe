#pragma once

#include "sdl-imgui.hxx" 
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ctime>
#include <cmath>

namespace globe
{
    struct Vertex
    {
        float x, y, z;    // Position
        float nx, ny, nz; // Normal (for lighting)
        float u, v;       // Texture Coordinates
    };

    class Globe
    {
        public:
            void GLobeInit (globe::Vertex vertex, GLuint image_texture);
            void globeLighting();
            void generateSphere (int subdivisions, std::vector<globe::Vertex>& vertices, std::vector<unsigned int>& indices);
    };
}
