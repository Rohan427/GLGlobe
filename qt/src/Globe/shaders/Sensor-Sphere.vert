#version 430 core

layout(location = 0) in vec3 aPos; // Raw model position vector from m_vbo

uniform mat4 model;      
uniform mat4 view;
uniform mat4 projection;
uniform float globeRadius; 

out vec3 vWorldPos;

void main() {
    // Strip your variable runtime globe radius configuration parameter.
    // This normalizes the vertex positions back to a perfect unit sphere baseline (radius = 1.0)
    vec3 unitPos = aPos / globeRadius;

    // Translate coordinates into active 3D World Space coordinates
    vec4 worldPos = model * vec4(unitPos, 1.0);
    vWorldPos = worldPos.xyz;
    
    gl_Position = projection * view * worldPos;
}
