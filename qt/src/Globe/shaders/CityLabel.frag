#version 430 core

// Texture unit 0 for the arial.png atlas
layout (binding = 0) uniform sampler2D fontAtlas;

in vec2 vTexCoord;
out vec4 FragColor;

// Constants for the "Halo" effect
const vec3 textColor = vec3 (1.0, 1.0, 1.0);    // White text
const vec3 outlineColor = vec3 (0.0, 0.0, 0.0); // Black outline

void main()
{
    // SDF distance is stored in the alpha channel
    float dist = texture (fontAtlas, vTexCoord).a;
    
    // Width of the anti-aliasing edge
    float smoothing = 0.02; 
    
    // Step 1: Calculate the inner text fill (Threshold at 0.5)
    float textAlpha = smoothstep (0.5 - smoothing, 0.5 + smoothing, dist);
    
    // Step 2: Calculate the outer outline (Threshold at 0.4)
    // Lowering the threshold to 0.35 would make the outline thicker
    float outlineAlpha = smoothstep (0.4 - smoothing, 0.4 + smoothing, dist);
    
    // Step 3: Layer the white text over the black outline
    vec3 finalRGB = mix (outlineColor, textColor, textAlpha);
    
    // Step 4: Output the pixel
    FragColor = vec4 (finalRGB, outlineAlpha);
    
    // Discard empty fragments to save fill-rate on the 7800 XT
    if (outlineAlpha < 0.05) discard;
}
