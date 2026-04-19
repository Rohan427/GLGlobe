#version 130

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D continentTexture; // From ROCm
uniform vec3 lightPos;              // Position of the "Sun"
uniform vec3 lightColor;            // Usually white (1.0, 1.0, 1.0)
uniform float ambientStrength;      // Min brightness (e.g., 0.2)

void main()
{
    // 1. Ambient Lighting (the "dark side" isn't pitch black)
    vec3 ambient = ambientStrength * lightColor;
  	
    // 2. Diffuse Lighting
    vec3 norm = normalize (Normal);
    vec3 lightDir = normalize (lightPos - FragPos);
    float diff = max(dot (norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // 3. Combine with ROCm Texture
    vec4 texColor = texture (continentTexture, TexCoord);
    vec3 result = (ambient + diffuse) * texColor.rgb;
    
    FragColor = vec4 (result, texColor.a);
}

