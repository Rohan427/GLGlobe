#version 460 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 tex;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec2 vTex;
layout (location = 1) out float vDiffuse;

layout (location = 0) uniform mat4 mvp;
layout (location = 4) uniform vec3 sunDirection;
layout (location = 5) uniform mat4 modelMatrix;

void main()
{
    vTex = tex; //vTex = vec2 (tex.x, 1.0 - tex.y); // vTex = tex;
    // Transform normal to World Space
    vec3 worldNormal = normalize (mat3 (modelMatrix) * normal);

    // Light is calculated against the fixed Sun direction
    vDiffuse = max (dot (worldNormal, normalize (sunDirection)), 0.0);

    gl_Position = mvp * vec4 (pos, 1.0);
}
