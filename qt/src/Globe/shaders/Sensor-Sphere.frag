#version 430 core

in vec3 vWorldPos;

uniform vec3 cameraWorldPos; 
uniform vec3 filterCenter;      // Fully transformed world coordinate matrix
uniform vec3 rangeRingColor;

out vec4 fragColor;

void main() {
    // 1. DYNAMIC TANGENT HORIZON PLANE CLIPPING
    vec3 toFragment = vWorldPos - filterCenter;
    vec3 planeNormal = normalize(filterCenter); // Points straight up out from Earth center (0,0,0)
    
    // Discard any fragment projecting beneath the local tangent horizon line
    if (dot(toFragment, planeNormal) < -0.0005) {
        discard;
    }

    // 2. PRODUCTION FIX: SCALE-INDEPENDENT PROCEDURAL NORMAL GENERATION
    // Since these assets are mathematically perfect spheres centered at filterCenter, 
    // the surface normal at any point is simply the normalized direction vector pointing 
    // from the sphere center out to the current fragment coordinate.
    // This completely bypasses any broken, scaled matrix transformations.
    vec3 normal = normalize(toFragment);

    // 3. CAMERA PERIMETER SILHOUETTE DETECTION (FRESNEL EFFECT)
    vec3 viewDir = normalize(cameraWorldPos - vWorldPos);

    // 0.0 means the eye vector is perfectly perpendicular to the surface normal (the true profile edge)
    float edgeFactor = abs(dot(viewDir, normal));

    // Screen space pixel variations for smooth anti-aliased line rendering
    float pixelDelta = fwidth(edgeFactor);
    
    // Dynamic safety fallback to ensure visibility at high resolutions/refresh rates
    float thickness  = max(pixelDelta * 2.5, 0.025); 

    float edgeAlpha = smoothstep(thickness, thickness - pixelDelta, edgeFactor);

    // Discard transparent interior fragment areas cleanly
    if (edgeAlpha <= 0.001) {
        discard;
    }

    fragColor = vec4(rangeRingColor, edgeAlpha);
}
