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
uniform mat4 viewMatrix;        // Need this for the rim logic

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
    vec3 bumpedNormal = normalize (vNormal + bump.x * cross (vNormal, vec3 (0, 1, 0)) + bump.y * vec3 (0, 1, 0));

    // 3. New Lighting Calculation
    float diffuse = max (dot (bumpedNormal, normalize (sunDirection)), 0.0);



/*
    // --- Specular logic ---
    vec3 worldViewDir = normalize (vec3 (0, 0, 10) - vPos); // Simple world-space view
    vec3 reflectDir = reflect (-normalize (sunDirection), bumpedNormal);
    float spec = pow (max (dot (worldViewDir, reflectDir), 0.0), 32.0);
    float sunMask = smoothstep (0.0, 0.05, diffuse); 

    // 3. Atmosphere Rim (View Space Fix)
    // We transform the normal to View Space just for this calculation
    vec3 eyeNormal = normalize (mat3 (viewMatrix) * bumpedNormal);

    // In View Space, the edge is where the normal is perpendicular to Z (0, 0, 1)
    float fresnel = 1.0 - max (dot (eyeNormal, vec3 (0, 0, 1)), 0.0);
    float rim = pow (fresnel, 300.0);
    float atmosphereMask = smoothstep (0.0, 0.15, diffuse);

    // 4. Final Color Assembly
    vec3 dayColor = texture (daySampler, vTex).rgb;
    vec3 nightColor = texture (nightSampler, vTex).rgb;
    
    // Day side + Specular
    vec3 daySide = dayColor * (diffuse + ambientIntensity);

    if (dayColor.b > dayColor.r && dayColor.b > dayColor.g)
    {
        daySide += vec3 (0.1) * spec * sunMask; 
    }

    float nightFactor = 1.0 - smoothstep (0.0, 0.15, diffuse);
    vec3 finalRGB = mix (daySide, nightColor * 2.0, nightFactor);

    // Add Atmosphere Halo
    finalRGB += vec3 (0.3, 0.6, 1.0) * rim * atmosphereMask * 0.01;

    // Add a subtle contrast boost (Reinhardt Tone Mapping lite)
    finalRGB = finalRGB / (finalRGB + vec3 (0.05)); 

    fragColor = vec4 (clamp (finalRGB, 0.0, 1.0), 1.0);
*/





    // 4. Standard Blending
    vec3 dayColor = texture (daySampler, vTex).rgb;
    vec3 nightColor = texture (nightSampler, vTex).rgb;
    
    float nightFactor = 1.0 - smoothstep (0.0, 0.15, diffuse);
    
    vec3 finalRGB = mix (dayColor * (diffuse + ambientIntensity), nightColor * 2.0, nightFactor);

    // Add this for a 'Water Reflection' effect
    vec3 viewDir = normalize (vec3 (0, 0, 1)); // Assuming camera is front-facing
    vec3 reflectDir = reflect (-normalize (sunDirection), bumpedNormal);

    // Use a power of 32 for a 'tight' ocean reflection
    float specIntensity = pow (max (dot (viewDir, reflectDir), 0.0), 32.0);

    // 2. MASK: Only apply if it's water AND it's in the sun
    // We use a small 'smoothstep' on vDiffuse to kill the shine at the terminator
    float sunMask = smoothstep (0.0, 0.05, diffuse); 

    // Only apply shine if it's water (Day map alpha or a specific blue threshold)
    if (dayColor.b > dayColor.r && dayColor.b > dayColor.g)
    {
        // Multiply spec by sunMask to kill the reflection on the dark side
        finalRGB += vec3 (0.4) * specIntensity * sunMask; 
    }

    fragColor = vec4 (finalRGB, 1.0);

}

