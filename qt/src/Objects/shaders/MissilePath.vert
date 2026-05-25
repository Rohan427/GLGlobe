#version 460 core

struct PathVertex
{
    vec4 position; // xyz = Coordinate on the curve, w = Alpha/Fade factor
};

// Trajectory Memory Link: Maps to a dedicated second binding slot on your graphics card
layout(std430, binding = 1) readonly buffer TrajectoryBlock
{
    PathVertex trailPoints[];
};

// Hardcoded Uniform Locations matching C++ register standards
layout (location = 0) uniform mat4 mvp;

// Output to Fragment Shader
layout (location = 0) out float vFade;

void main()
{
    // gl_VertexID identifies exactly which point on the curve we are drawing
    vec3 worldPos = trailPoints[gl_VertexID].position.xyz;
    vFade         = trailPoints[gl_VertexID].position.w;

    // Apply the transformation matrix to project the coordinate onto the 4K viewport
    gl_Position = mvp * vec4 (worldPos, 1.0f);
}
