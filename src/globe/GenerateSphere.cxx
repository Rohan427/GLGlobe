#include "Globe.hxx"

/* To implement the 3D globe, the following function is used to generate the necessary vertex positions (x, y, z) and 
   texture coordinates (u, v) for a UV Sphere. This data can then be passed to the OpenGL buffers. 

   This function creates a grid of vertices based on a specified number of subdivisions. Higher values result in a
   smoother globe.

   Buffer Upload: Use glGenBuffers and glBufferData to send this generated data to the GPU in a Vertex Buffer Object
                  (VBO) and Element Buffer Object (EBO).

   Shader Mapping: Your ROCm-generated image will act as the "skin." Ensure the kernel produces an image with a 2:1
                   aspect ratio (e.g., 2048x1024) to match the equirectangular projection used by the UV coordinates.

   Interaction: When the user rotates the globe, update the model matrix in the vertex shader using the yaw and pitch
                values captured from the SDL mouse events.

   Quick Reference for RHEL 10 Setup
   ---------------------------------
   Library: Uses the GLM Math Library for easy matrix operations like glm::perspective and glm::rotate. It is often
            available via dnf install glm-devel on RHEL systems.
 */

void generateSphere (int subdivisions, std::vector<globe::Vertex>& vertices, std::vector<unsigned int>& indices)
{
    const float PI = 3.14159265359f;

    for (int y = 0; y <= subdivisions; ++y)
    {
        for (int x = 0; x <= subdivisions; ++x)
        {
            float py = (float)y / subdivisions;
            float px = (float)x / subdivisions;

            float phi = py * PI;
            float theta = px * 2.0f * PI;

            float vx = std::sin (phi) * std::cos (theta);
            float vy = std::cos (phi);
            float vz = std::sin (phi) * std::sin (theta);

            // For a unit sphere, the position is the normal
            vertices.push_back ({vx, vy, vz, vx, vy, vz, px, py});
        }
    }

    // Index generation remains the same as previous step
    for (int y = 0; y < subdivisions; ++y)
    {
        for (int x = 0; x < subdivisions; ++x)
        {
            unsigned int p1 = y * (subdivisions + 1) + x;
            unsigned int p2 = p1 + (subdivisions + 1);

            indices.push_back (p1); indices.push_back (p2); indices.push_back (p1 + 1);
            indices.push_back (p1 + 1); indices.push_back (p2); indices.push_back (p2 + 1);
        }
    }
}
