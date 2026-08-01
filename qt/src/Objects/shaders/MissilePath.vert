#version 460 core

layout (location = 0) in vec3 aPos;        // Per-point position from CPU

layout (location = 0) uniform mat4 mvp;
layout (location = 7) uniform vec4 trailColor;   // rgba

layout (location = 0) out float vFade;     // Pass fade to fragment

void main()
{
    gl_Position = mvp * vec4 (aPos, 1.0);
    gl_PointSize = 5.0;                    // Adjustable trail dot size
    
    // Simple age-based fade (we'll pass vertex index via gl_VertexID if needed)
    vFade = 1.0;
}
