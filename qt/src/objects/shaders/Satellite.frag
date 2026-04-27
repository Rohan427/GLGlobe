#version 430 core

out vec4 fragColor;

uniform vec3 satColor; // Pass this from the sidebar later!

void main()
{
    // Basic solid color
    fragColor = vec4 (satColor, 1.0);
}
