#version 460 core

layout (location = 0) in vec3 vWorldPos;

layout (location = 13) uniform vec3 cameraWorldPos; 
layout (location = 14) uniform vec3 filterCenter;      // Fully transformed world coordinate matrix
layout (location = 15) uniform vec3 rangeRingColor;

layout (location = 0) out vec4 fragColor;

void main()
{
    // 1. DYNAMIC TANGENT HORIZON PLANE CLIPPING
    vec3 toFragment = vWorldPos - filterCenter;
    vec3 planeNormal = normalize (filterCenter); // Points straight up out from Earth center (0,0,0)

    // Evaluate the perpendicular height of the current fragment above the tangent plane
    float heightAbovePlane = dot (toFragment, planeNormal);
    
    /// Hard clip for anything below the horizon plane
    if (heightAbovePlane < -0.0005)
    {
        discard;
    }

    // 2. PRODUCTION FIX: SCALE-INDEPENDENT PROCEDURAL NORMAL GENERATION
    // Since these assets are mathematically perfect spheres centered at filterCenter, 
    // the surface normal at any point is simply the normalized direction vector pointing 
    // from the sphere center out to the current fragment coordinate.
    // This completely bypasses any broken, scaled matrix transformations.
    vec3 normal = normalize (toFragment);

    // 3. CAMERA PERIMETER SILHOUETTE DETECTION (FRESNEL EFFECT)
    vec3 viewDir = normalize (cameraWorldPos - vWorldPos);

    // 0.0 means the eye vector is perfectly perpendicular to the surface normal (the true profile edge)
    float edgeFactor = abs (dot (viewDir, normal));

    // Dynamic camera distance-compensated line thickness tracking loop for 4K display monitors
    float camDist = length (cameraWorldPos - vWorldPos);
    float pixelDelta = fwidth (edgeFactor);
    float thickness = max (pixelDelta * 2.5, camDist * 0.008); 

    // Generate alpha layer mapping for the floating concentric 3D dome rings
    float domeRingAlpha = smoothstep (thickness, thickness - (pixelDelta * 2.0), edgeFactor);

    // =========================================================================
    // 3. NEW: TANGENT HORIZON GROUND FOOTPRINT CIRCLE MATH
    // =========================================================================
    // Project the fragment position onto the flat tangent plane surface
    vec3 projectedOnPlane = toFragment - (heightAbovePlane * planeNormal);
    
    // Calculate the horizontal 2D distance from the city anchor center to this point on the plane
    float flatGroundRadius = length (projectedOnPlane);

    // Determine how close this fragment is to the outer maximum sensor coverage rim
    // Because the spheres are scaled uniformly, the sphere with radius R meets the plane at radius R
    float distToGroundOuterRim = abs (flatGroundRadius - length(toFragment));

    // Compute localized screen space derivatives for the flat plane intersection
    float groundDelta = fwidth (distToGroundOuterRim);
    float groundThickness = max (groundDelta * 2.5, camDist * 0.008);

    // Generate a sharp ground ring at the boundary where the dome meets the Earth
    float groundRingAlpha = smoothstep (groundThickness, groundThickness - (groundDelta * 2.0), distToGroundOuterRim);

    // Fade the ground ring as the sphere curves upward away from the surface 
    // This locks the ring strictly to the base intersection seam line
    float groundMask = smoothstep (groundThickness * 1.5, 0.0, heightAbovePlane);
    groundRingAlpha *= groundMask;

    // =========================================================================
    // 4. COMPOSITING THE SENSOR INTERSECTIONS
    // =========================================================================
    // Combine the vertical silhouette domes and the flat plane ground circle
    float finalRingAlpha = max (domeRingAlpha, groundRingAlpha);

    if (finalRingAlpha <= 0.001)
    {
        discard;
    }

    fragColor = vec4 (rangeRingColor, finalRingAlpha);
}
