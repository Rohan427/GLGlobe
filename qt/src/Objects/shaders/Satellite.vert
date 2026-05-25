#version 460 core

struct GpuEntityData {
    vec4 position;  // xyz = Coordinate [X,Y,Z] | w = Scale
    vec4 velocity;  // xyz = Direction Vector  | w = Status Flag (1.0 = Live, 0.0 = Debris)
    vec4 metadata;  // x = Lifespan Decay Timer | y = Mass Parameter
};

// Map directly to global layout binding slot 0
layout(std430, binding = 0) readonly buffer SimulationBlock {
    GpuEntityData entities[];
};

// Hardcoded Uniform Locations matching C++
layout (location = 0) uniform mat4 mvp;
layout (location = 4) uniform vec3 filterCenter;
layout (location = 5) uniform float filterRadius;
layout (location = 6) uniform bool filterEnabled;
layout (location = 7) uniform vec4 satColor;

layout (location = 0) out vec4 vColor;

void main()
{
    // Extract positions and active status parameters from the SSBO using the vertex ID
    vec3 currentPos = entities[gl_VertexID].position.xyz;
    float status    = entities[gl_VertexID].velocity.w;

    // Apply color parameters dynamically based on tracking type
    if (status > 0.5f)
    {
        vColor = satColor; // Active Satellite (Magenta)
    }
    else
    {
        vColor = vec4 (1.0f, 0.4f, 0.0f, 0.7f); // Debris Particle (Orange Glow)
    }

    // Assign visible point sizing
    gl_PointSize = (status > 0.5f) ? 8.0f : 4.0f;

    // Handle sensor filter masking calculations locally
    if (filterEnabled)
    {
        // --- A. HORIZONTAL MAX SENSOR RANGE THRESHOLD CHECK ---
        float dist = distance (currentPos, filterCenter);

        if (dist > filterRadius)
        {
            gl_Position = vec4 (0.0f, 0.0f, 0.0f, 0.0f); // Collapse vertex off-screen

            return;
        }

        // --- B. REPAIR: FLAT TANGENT HORIZON PLANE GUARD ---
        // Project the satellite position vector relative to the city anchor
        vec3 toSatellite = currentPos - filterCenter;
        vec3 planeNormal = normalize (filterCenter); // The upward surface normal vector
        
        // Evaluate the perpendicular height of the satellite above the tangent plane
        // If the dot product is negative, the satellite is physically beneath the horizon.
        if (dot (toSatellite, planeNormal) < -0.0005)
        {
            gl_Position = vec4 (0.0f, 0.0f, 0.0f, 0.0f); // Collapse vertex off-screen
            return;
        }
    }

    // Apply projection transforms cleanly
    gl_Position = mvp * vec4 (currentPos, 1.0f);
}

