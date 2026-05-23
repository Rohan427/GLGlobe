#version 460 core

layout (location = 0) in vec2 vTex;
layout (location = 1) in float vDiffuse;

layout (location = 0) out vec4 fragColor;

layout (location = 0) uniform sampler2D sampler;

void main()
{
    vec4 texColor = texture(sampler, vTex);
    float ambient = 0.15; // The dark side brightness
    float light = clamp(vDiffuse + ambient, 0.0, 1.0);
    fragColor = vec4(texColor.rgb * light, texColor.a);
}
