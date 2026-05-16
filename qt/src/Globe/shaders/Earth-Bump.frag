#version 430 core

in vec2 vTex;
in vec3 vNormal;
in vec3 vPos;

// For sensor range visuals
uniform vec3 filterCenter;
uniform float filterRadius;
uniform bool filterEnabled;
uniform vec3 rangeRingColor;
uniform float rangeRingDelta; // Spacing in GL units

out vec4 fragColor;

uniform sampler2D daySampler;
uniform sampler2D nightSampler;
uniform sampler2D bumpSampler;
uniform vec3 sunDirection;
uniform float ambientIntensity;

void main()
{
    // Sample the bump map to find the slope
    float texelSize = 1.0 / 8192.0; // Should probably change this to a parameter for different map sizes
    float hL = texture (bumpSampler, vTex + vec2 (-texelSize, 0.0)).r;
    float hR = texture (bumpSampler, vTex + vec2 (texelSize, 0.0)).r;
    float hD = texture (bumpSampler, vTex + vec2 (0.0, -texelSize)).r;
    float hU = texture (bumpSampler, vTex + vec2 (0.0, texelSize)).r;

    // Perturb the normal
    float strength = 10.0; // Higher = flatter mountains
    vec3 bump = normalize (vec3 (hL - hR, hD - hU, 1.0 / strength));
    
    // Reconstruct a new normal based on the sphere's surface + the bump
    // We use a simplified TBN-style approach suitable for a sphere
    vec3 bumpedNormal = normalize (vNormal + bump.x * cross (vNormal, vec3 (0, 1, 0)) + bump.y * vec3 (0, 1, 0));

    // Lighting Calculation
    float diffuse = max (dot (bumpedNormal, normalize (sunDirection)), 0.0);

    // Standard Blending
    vec3 dayColor = texture (daySampler, vTex).rgb;
    vec3 nightColor = texture (nightSampler, vTex).rgb;
    
    float nightFactor = 1.0 - smoothstep (0.0, 0.15, diffuse);
    
    vec3 finalRGB = mix (dayColor * (diffuse + ambientIntensity), nightColor * 2.0, nightFactor);

    // Add this for a 'Water Reflection' effect
    vec3 viewDir = normalize (vec3 (0, 0, 1)); // Assuming camera is front-facing
    vec3 reflectDir = reflect (-normalize (sunDirection), bumpedNormal);

    // Use a power of 32 for a 'tight' ocean reflection
    float specIntensity = pow (max (dot (viewDir, reflectDir), 0.0), 32.0);

    // MASK: Only apply if it's water AND it's in the sun
    // We use a small 'smoothstep' on vDiffuse to kill the shine at the terminator
    float sunMask = smoothstep (0.0, 0.05, diffuse); 

    // Only apply shine if it's water (Day map alpha or a specific blue threshold)
    if (dayColor.b > dayColor.r && dayColor.b > dayColor.g)
    {
        // Multiply spec by sunMask to kill the reflection on the dark side
        finalRGB += vec3 (0.4) * specIntensity * sunMask; 
    }

    

    // Sensor filter
    //if (filterEnabled)
    //{
    //    float dist = distance (vPos, filterCenter);

    //    if (dist <= filterRadius)
    //    {
    //        // Range Ring Math
    //        float ringValue = mod (dist, rangeRingDelta);
    //        float thickness = 0.003; // Adjust for 4K visibility
    //        
    //        // Use smoothstep for anti-aliased, crisp rings at 1100 FPS
    //        float ringAA = smoothstep (thickness, thickness - 0.001, ringValue);
    //        
    //        // Blend the ring color over the map texture
    //        finalRGB.rgb = mix (finalRGB.rgb, rangeRingColor, ringAA);
    //    }
    //}

    fragColor = vec4 (finalRGB, 1.0);
}

