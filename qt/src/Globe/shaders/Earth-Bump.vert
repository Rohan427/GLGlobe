#version 430 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 tex;
layout (location = 2) in vec3 normal;

out vec2 vTex;
out vec3 vNormal; // Added for bump mapping
out vec3 vPos;    // Added for bump mapping

uniform mat4 mvp;
uniform mat4 modelMatrix;

void main()
{
    vTex = tex;
    
    // Pass the Normal and Position in World Space
    vNormal = normalize (mat3 (modelMatrix) * normal);
    vPos = vec3 (modelMatrix * vec4 (pos, 1.0));
    
    gl_Position = mvp * vec4 (pos, 1.0);
}
