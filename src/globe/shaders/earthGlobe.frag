#version 130

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D continentTexture;
uniform vec3 lightPos; // Sent from C++

void main()
{
    vec3 norm = normalize (Normal);
    vec3 lightDir = normalize (lightPos - FragPos);

    // Calculate Diffuse Lighting
    float diff = max (dot (norm, lightDir), 0.0);

    // Set Ambient (Night) light level - 0.05 is very dark, 0.2 is visible
    float ambient = 0.1;
    float lighting = max (diff, ambient);

    vec4 texColor = texture (continentTexture, TexCoord);
    
    // Apply lighting to the continent colors
    FragColor = vec4 (texColor.rgb * lighting, 1.0);
}

