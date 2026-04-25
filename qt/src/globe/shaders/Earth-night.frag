#version 430 core

in vec2 vTex;
in float vDiffuse;
out vec4 fragColor;

uniform sampler2D daySampler;   // Your Natural Earth III day map
uniform sampler2D nightSampler; // The Natural Earth III night map

void main()
{
    vec4 dayColor = texture (daySampler, vTex);
    vec4 nightColor = texture (nightSampler, vTex);

    // vDiffuse is 1.0 in full sun and 0.0 in the dark.
    // We mix based on the 'night' factor.
    float nightFactor = 1.0 - smoothstep (0.0, 0.2, vDiffuse);
    
    // Boost the night light brightness a bit for visibility
    vec3 finalRGB = mix (dayColor.rgb * (vDiffuse + 0.15), nightColor.rgb * 1.5, nightFactor);
    
    fragColor = vec4 (finalRGB, 1.0);
}

