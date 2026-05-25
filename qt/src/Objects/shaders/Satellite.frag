#version 460 core

layout (location = 0) in vec4 vColor; // Fed straight from vertex shader output location 0
layout (location = 0) out vec4 fragColor;

void main()
{
    fragColor = vColor;
}
