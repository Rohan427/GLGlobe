#version 460 core

layout (location = 0) in vec3 aPos; // Raw model position vector from m_vbo

layout (location = 0) uniform mat4 model;      
layout (location = 4) uniform mat4 view;
layout (location = 8) uniform mat4 projection;
layout (location = 12) uniform float globeRadius; 

layout (location = 0) out vec3 vWorldPos;

void main()
{
    // Strip your variable runtime globe radius configuration parameter.
    // This normalizes the vertex positions back to a perfect unit sphere baseline (radius = 1.0)
    vec3 unitPos = aPos / globeRadius;

    // Translate coordinates into active 3D World Space coordinates
    vec4 worldPos = model * vec4 (unitPos, 1.0);
    vWorldPos = worldPos.xyz;
    
    gl_Position = projection * view * worldPos;
}
