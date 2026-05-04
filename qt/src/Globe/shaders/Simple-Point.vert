#version 430 core

layout (location = 0) in vec3 aPos;
uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vec4 (aPos, 1.0);
    gl_PointSize = 15.0; // Fixed size or scale based on zoom
}
