#version 430 core

in vec2 vTex;
in vec3 vNormal;
in vec3 vPos;

out vec4 fragColor;

uniform sampler2D daySampler;
uniform sampler2D nightSampler;
uniform sampler2D bumpSampler;
uniform vec3 sunDirection;
uniform float ambientIntensity;

void main()
{
    // 1. Sample the bump map to find the slope
    float texelSize = 1.0 / 8192.0; 
    float hL = texture (bumpSampler, vTex + vec2 (-texelSize, 0.0)).r;
    float hR = texture (bumpSampler, vTex + vec2 (texelSize, 0.0)).r;
    float hD = texture (bumpSampler, vTex + vec2 (0.0, -texelSize)).r;
    float hU = texture (bumpSampler, vTex + vec2 (0.0, texelSize)).r;

    // 2. Perturb the normal
    float strength = 10.0; // Higher = flatter mountains
    vec3 bump = normalize (vec3 (hL - hR, hD - hU, 1.0 / strength));
    
    // Reconstruct a new normal based on the sphere's surface + the bump
    // We use a simplified TBN-style approach suitable for a sphere
    vec3 bumpedNormal = normalize (vNormal + bump.x * cross (vNormal, vec3 (0,1,0)) + bump.y * vec3 (0,1,0));

    // 3. New Lighting Calculation
    float diffuse = max (dot (bumpedNormal, normalize (sunDirection)), 0.0);

    // 4. Standard Blending
    vec3 dayColor = texture (daySampler, vTex).rgb;
    vec3 nightColor = texture (nightSampler, vTex).rgb;
    
    float nightFactor = 1.0 - smoothstep (0.0, 0.15, diffuse);
    
    vec3 finalRGB = mix (dayColor * (diffuse + ambientIntensity), nightColor * 2.0, nightFactor);

    // Add this for a 'Water Reflection' effect
    vec3 viewDir = normalize (vec3 (0,0,1)); // Assuming camera is front-facing
    vec3 reflectDir = reflect (-normalize(sunDirection), bumpedNormal);
    float spec = pow (max (dot (viewDir, reflectDir), 0.0), 32.0);

    // Only apply shine if it's water (Day map alpha or a specific blue threshold)
    if (dayColor.b > dayColor.r && dayColor.b > dayColor.g)
    {
        finalRGB += vec3 (0.3) * spec; 
    }

    fragColor = vec4 (finalRGB, 1.0);
}

