#version 430 core

in vec2 vTex;
in float vDiffuse;
out vec4 fragColor;
uniform sampler2D sampler;

void main()
{
    vec4 texColor = texture(sampler, vTex);
    float ambient = 0.15; // The dark side brightness
    float light = clamp(vDiffuse + ambient, 0.0, 1.0);
    fragColor = vec4(texColor.rgb * light, texColor.a);
}
