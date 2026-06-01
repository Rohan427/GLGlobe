#version 460 core

// Input matched exactly by name and location from Vertex Shader
layout (location = 0) in float vFade;

// Fragment Uniform Register Spaces
layout (location = 7) uniform vec4 missileColor; // e.g., Electric Blue (0.0, 0.8, 1.0, 1.0)

layout (location = 0) out vec4 fragColor;

void main()
{
    // Apply the alpha fade factor procedurally across the line segments
    fragColor = vec4 (missileColor.rgb, missileColor.a * vFade);
}
