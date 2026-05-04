#version 430 core

layout (location = 0) in vec3 anchor; // City XYZ
layout (location = 1) in vec2 uv;     // Font UV
layout (location = 2) in vec2 offset; // Pixel offset for letter position

uniform mat4 mvp;      // Your standard projection * view * model
uniform float scale;   // Controls text size based on zoom

out vec2 vTexCoord;

void main()
{
    vTexCoord = uv;

    // 1. Project the anchor point (the city's location)
    vec4 projectedPos = mvp * vec4 (anchor, 1.0);

    // 2. Add the billboard offset in Clip Space
    // We multiply by projectedPos.w to ensure the offset stays 
    // proportional to the perspective (so text doesn't look giant when far away)
    gl_Position = projectedPos;
    gl_Position.xy += offset * scale * projectedPos.w;
}
