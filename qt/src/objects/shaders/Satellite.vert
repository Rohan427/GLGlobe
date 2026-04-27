#version 430 core

layout (location = 0) in vec3 pos; // Satellite world position
uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vec4 (pos, 1.0);
    // Control point size directly in the shader
    gl_PointSize = 10.0; 
}
