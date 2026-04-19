#version 130

// Input from the generateSphere function
in vec3 aPos;       // x, y, z
in vec2 aTexCoord;  // u, v

out vec2 TexCoord;

// Uniforms updated by SDL mouse events
uniform mat4 model;      // Combined Yaw and Pitch rotation matrices
uniform mat4 view;       // Translation matrix for Zoom (Z) and Panning (X, Y)
uniform mat4 projection; // Perspective matrix (e.g., 45.0f FOV)

void main()
{
    // Standard 3D transformation pipeline
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    
    // Pass UVs to Fragment Shader for the ROCm texture
    TexCoord = aTexCoord;
}
