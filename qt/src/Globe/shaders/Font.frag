#version 430 core

// Font.frag
uniform sampler2D fontAtlas;
varying vec2 vTexCoord;

/* Simple shader
void main()
{
    float distance = texture2D(fontAtlas, vTexCoord).a;
    
    // Smoothstep creates the anti-aliased edge
    // 0.5 is the "edge" of the character
    float smoothing = 0.1; // Adjust based on zoom/font size
    float alpha = smoothstep (0.5 - smoothing, 0.5 + smoothing, distance);
    
    gl_FragColor = vec4 (1.0, 1.0, 1.0, alpha); 
}
*/

/* Complex shader */
uniform sampler2D fontAtlas;
varying vec2 vTexCoord;

// Colors for high contrast
const vec4 textColor = vec4 (1.0, 1.0, 1.0, 1.0); // White
const vec4 outlineColor = vec4 (0.0, 0.0, 0.0, 1.0); // Black

void main()
{
    float distance = texture (fontAtlas, vTexCoord).a;
    
    // inner edge (text fill)
    float edge = 0.5;
    float smoothing = 0.05; // Adjust for crispness
    
    // outer edge (outline)
    float outlineEdge = 0.4; // Smaller value = thicker outline
    
    float alpha = smoothstep (edge - smoothing, edge + smoothing, distance);
    float outlineAlpha = smoothstep (outlineEdge - smoothing, outlineEdge + smoothing, distance);
    
    // Mix the outline behind the text
    vec4 finalColor = mix(outlineColor, textColor, alpha);
    gl_FragColor = vec4 (finalColor.rgb, outlineAlpha);
}
