#version 460 core

layout (location = 0) out vec4 fragColor;

layout (location = 7) uniform vec3 satColor; // Pass this from the sidebar later!

void main()
{
    // Basic solid color
    fragColor = vec4 (satColor, 1.0);
}
