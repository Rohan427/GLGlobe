#version 460 core

layout (location = 0) in vec3 anchor;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec2 offset; // These are pixels (e.g., 40.0)

layout (location = 0) uniform mat4 mvp;
layout (location = 4) uniform vec2 viewportSize; // Pass your window width/height (e.g., 3840, 2160)
layout (location = 5) uniform float scale;       // Controls overall label size (start with 1.0)

layout (location = 0) out vec2 vTexCoord;

void main()
{
    vTexCoord = uv;
    vec4 projectedPos = mvp * vec4(anchor, 1.0);

    // 1. Convert pixel offsets to Normalized Device Coordinates
    // Divide by viewportSize to turn pixels into a ratio (0.0 to 1.0)
    // Multiply by 2.0 because NDC space is 2.0 units wide (-1 to 1)
    vec2 ndcOffset = (offset * scale * 2.0) / viewportSize;

    // 2. Apply the offset
    // Multiplying by projectedPos.w keeps the text size "constant" in perspective
    gl_Position = projectedPos;
    gl_Position.xy += ndcOffset * projectedPos.w;
}

