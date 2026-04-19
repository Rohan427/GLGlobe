/* Integration
   Coordinate Sync: Ensure your generateSphere function uses a right-handed coordinate system to match GLM's defaults.

   Resizing: When the user resizes the SDL window, recalculate the aspect ratio for the projection matrix so the globe  
             doesn't look like an egg.

   Depth Testing: Don't forget to call glEnable (GL_DEPTH_TEST); before drawing the sphere, otherwise the "back" of the
                  globe will draw over the "front."

*/

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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

// 4. Send to Shader
glUniformMatrix4fv (glGetUniformLocation (shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr (model));
glUniformMatrix4fv (glGetUniformLocation (shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr (view));
glUniformMatrix4fv (glGetUniformLocation (shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr (projection));
