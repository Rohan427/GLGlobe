#version 130

in vec2 TexCoord; // UV (0.0 to 1.0)
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D continentTexture; // Your ROCm/HIP generated texture

void main()
{
    // 1. Handle Longitude Wrap:
    // Ensure U (longitude) wraps cleanly from 1.0 back to 0.0
    vec2 wrappedUV = vec2 (fract (TexCoord.x), TexCoord.y);

    // 2. Simple Lighting (as established previously)
    vec3 lightDir = normalize (vec3 (5.0, 5.0, 5.0) - FragPos);
    float diff = max (dot (normalize (Normal), lightDir), 0.2); // 0.2 is ambient

    // 3. Sample the Accuracy Map
    vec4 texColor = texture (continentTexture, wrappedUV);
    
    // Multiply by lighting for 3D depth
    FragColor = vec4 (texColor.rgb * diff, 1.0);
}
