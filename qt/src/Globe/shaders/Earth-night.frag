#version 460 core

layout (location = 0) in vec2 vTex;
layout (location = 1) in float vDiffuse;

layout (location = 0) out vec4 fragColor;

layout (location = 0) uniform sampler2D daySampler;   // The Natural Earth III day map
layout (location = 1) uniform sampler2D nightSampler; // The Natural Earth III night map
layout (location = 2) uniform float ambientIntensity; // Ambient uniform

void main()
{
    // 1. Get colors as vec4 first
    vec4 dayData = texture(daySampler, vTex);
    vec4 nightData = texture(nightSampler, vTex);

    // 2. Extract RGB for the math (vec3)
    vec3 dayRGB = dayData.rgb;
    vec3 nightRGB = nightData.rgb;

    // 3. Calculate blend factor
    float nightFactor = 1.0 - smoothstep(0.0, 0.15, vDiffuse);
    
    // 4. Perform the mix (all vec3)
    vec3 daySide = dayRGB * (vDiffuse + ambientIntensity);
    vec3 nightSide = nightRGB * 2.0; 
    vec3 finalRGB = mix(daySide, nightSide, nightFactor);

    // 5. Assign to the out vec4
    fragColor = vec4(finalRGB, 1.0);
}

