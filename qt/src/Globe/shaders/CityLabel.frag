#version 460 core


// Texture unit 0 for the arial.png atlas
layout (binding = 0) uniform sampler2D fontAtlas;

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) out vec4 FragColor;

// Constants for the "Halo" effect
const vec3 textColor = vec3 (1.0, 1.0, 1.0);    // White text
const vec3 outlineColor = vec3 (0.0, 0.0, 0.0); // Black outline

void main()
{
    // SDF distance is stored in the alpha channel
    float dist = texture (fontAtlas, vTexCoord).a;

    // Width of the anti-aliasing edge
    float smoothing = 0.01;

    // Step 1: Calculate the inner text fill (Threshold at 0.5)
    float textAlpha = smoothstep (0.5 - smoothing, 0.5 + smoothing, dist);

    // Step 2: Calculate the outer outline (Threshold at 0.4)
    // Lowering the threshold to 0.35 would make the outline thicker
    float outlineAlpha = smoothstep (0.4 - smoothing, 0.4 + smoothing, dist);

    // Step 3: Layer the white text over the black outline
    vec3 finalRGB = mix (outlineColor, textColor, textAlpha);

    // Step 4: Output the pixel
    // The next 2 lines are to clamp the label when zoomed out
//    float fade = clamp(1.0 - (distToCamera / maxDistance), 0.0, 1.0);
//    FragColor = vec4 (finalRGB, outlineAlpha * fade);

    FragColor = vec4 (finalRGB, outlineAlpha); // Actual
    ////FragColor = vec4 (1.0, 0.0, 0.0, 1.0); // Test red block
    ////FragColor = vec4(vTexCoord.x, vTexCoord.y, 0.0, 1.0); // test U V color

    // Discard empty fragments to save fill-rate on the 7800 XT
    if (outlineAlpha < 0.05) discard;
}
