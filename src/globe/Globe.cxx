#include "Globe.hxx"

using namespace globe;

void Globe::GLobeInit (globe::Vertex vertex, GLuint image_texture)
{
    // Position (Location 0)
    glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, sizeof (vertex), (void*)0);
    glEnableVertexAttribArray (0);

    // Normal (Location 1) - The New Part
    glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, sizeof (vertex), (void*)(3 * sizeof (float)));
    glEnableVertexAttribArray (1);

    // UV (Location 2) - Offset by 6 floats now
    glVertexAttribPointer (2, 2, GL_FLOAT, GL_FALSE, sizeof (vertex), (void*)(6 * sizeof (float)));
    glEnableVertexAttribArray (2);

    // Setup for smooth texture mapping onto the globe
    glBindTexture (GL_TEXTURE_2D, image_texture);

    // Horizontal (Longitude) - Repeat allows seamless 0/360 wrap
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

    // Vertical (Latitude) - Clamp to edge to prevent "bleeding" at the poles
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Use Linear filtering for smooth coastlines when zooming
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}


/* For dynamically lit globe (real-time lighting)

Implementation on RHEL 10
Performance: This calculation is negligible for the CPU. By using SDL_GetTicks(), the rotation remains smooth even if
         the framerate fluctuates.

Math Library: Ensure you have GLM installed (sudo dnf install glm-devel) and linked in your CMakeLists.txt using
          find_package(glm REQUIRED).

Coordinate System: Note that in this setup, the "Sun" moves around the globe. If you want the Sun to stay still and the
               Globe to rotate (to match Earth's actual rotation), apply the currentTime rotation to your Model
               Matrix instead of the lightPos.

*/

// --- Inside the Main Loop ---
void Globe::globeLighting()
{
    // 1. Get current time (or a manual timer for faster cycles)
    float timeScale = 10.0f; // Speed up the day/night cycle
    float currentTime = (float)SDL_GetTicks() / 1000.0f * timeScale;

    // 2. Calculate Light Position (Orbiting the Y-axis)
    float radius = 10.0f; 
    float lightX = sin (currentTime) * radius;
    float lightZ = cos (currentTime) * radius;
    float lightY = 2.0f; // Slight tilt for seasonal effect

    glm::vec3 dynamicLightPos (lightX, lightY, lightZ);

    // 3. Send to Fragment Shader (litGlobeContinent.frag)
    GLuint lightPosLoc = glGetUniformLocation (shaderProgram, "lightPos");
    glUniform3fv (lightPosLoc, 1, glm::value_ptr (dynamicLightPos));
}

void Globe::generateSphere (int subdivisions, std::vector<globe::Vertex>& vertices, std::vector<unsigned int>& indices)
{
    const float PI = 3.14159265359f;

    for (int y = 0; y <= subdivisions; ++y)
    {
        for (int x = 0; x <= subdivisions; ++x)
        {
            float py = (float)y / subdivisions;
            float px = (float)x / subdivisions;

            // Convert UV-style normalized coordinates to Spherical coordinates
            float phi = py * PI;            // Latitude (0 to PI)
            float theta = px * 2.0f * PI;   // Longitude (0 to 2*PI)

            // Calculate Cartesian positions
            float vx = std::sin (phi) * std::cos (theta);
            float vy = std::cos (phi);
            float vz = std::sin (phi) * std::sin (theta);

            // Texture coordinates (mapping the ROCm image to the sphere)
            vertices.push_back ({vx, vy, vz, px, py});
        }
    }

    // Generate indices for triangle strips
    for (int y = 0; y < subdivisions; ++y)
    {
        for (int x = 0; x < subdivisions; ++x)
        {
            unsigned int p1 = y * (subdivisions + 1) + x;
            unsigned int p2 = p1 + (subdivisions + 1);

            indices.push_back (p1);
            indices.push_back (p2);
            indices.push_back (p1 + 1);

            indices.push_back (p1 + 1);
            indices.push_back (p2);
            indices.push_back (p2 + 1);
        }
    }
}


void Globe::generateSphereNormals (int subdivisions, std::vector<globe::Vertex>& vertices, std::vector<unsigned int>& indices)
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

            indices.push_back (p1);
            indices.push_back (p2);
            indices.push_back (p1 + 1);

            indices.push_back (p1 + 1);
            indices.push_back (p2);
            indices.push_back (p2 + 1);
        }
    }
}


/* Integration
   Coordinate Sync: Ensure your generateSphere function uses a right-handed coordinate system to match GLM's defaults.

   Resizing: When the user resizes the SDL window, recalculate the aspect ratio for the projection matrix so the globe  
             doesn't look like an egg.

   Depth Testing: Don't forget to call glEnable (GL_DEPTH_TEST); before drawing the sphere, otherwise the "back" of the
                  globe will draw over the "front."

*/
void Globe::rotateSphere()
{
    // --- Inside the Render Loop ---

    // 1. Model Matrix: Rotation
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate (model, glm::radians (pitch), glm::vec3 (1.0f, 0.0f, 0.0f));
    model = glm::rotate (model, glm::radians (yaw),   glm::vec3 (0.0f, 1.0f, 0.0f));

    // 2. View Matrix: Zoom and Panning
    glm::mat4 view = glm::mat4 (1.0f);
    // Move camera back by 'zoom' and shift by 'offsetX/Y'
    view = glm::translate (view, glm::vec3 (offsetX, offsetY, zoom)); 

    // 3. Projection Matrix: 3D Depth
    float aspect = (float)pane_width / (float)pane_height;
    glm::mat4 projection = glm::perspective (glm::radians (45.0f), aspect, 0.1f, 100.0f);

    // 4. Send to Shader (globe.vert)
    glUniformMatrix4fv (glGetUniformLocation (shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr (model));
    glUniformMatrix4fv (glGetUniformLocation (shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr (view));
    glUniformMatrix4fv (glGetUniformLocation (shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr (projection));
}

