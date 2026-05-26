#version 460 core

struct GpuEntityData
{
    vec4 position;  // xyz = Coordinate [X,Y,Z] | w = Scale
    vec4 velocity;  // xyz = Direction Vector  | w = Status Flag (1.0 = Live, 0.0 = Debris)
    vec4 metadata;  // x = Lifespan Decay Timer | y = Mass Parameter
    vec4 padding;   // Matches C++ 64-byte struct stride layout footprint
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
    float typeId = entities[gl_VertexID].metadata.w;

    // Early exit: If the slot type identifier is empty/dead (0.0f), drop it immediately
    if (typeId == 0.0f)
    {
        gl_Position = vec4 (0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    // Extract positions and active status parameters from the SSBO using the vertex ID
    vec3 currentPos = entities[gl_VertexID].position.xyz;

    // =========================================================================
    // OPTIMIZATION FIRST: EARLY-EXIT FILTER GATES
    // =========================================================================
    if (filterEnabled)
    {
        // SAFETY GATE: If the position data collapses to the Earth core, discard instantly
        // This completely prevents division-by-zero crashes on hardware
        if (length(currentPos) < 0.01f)
        {
            gl_Position = vec4 (0.0f, 0.0f, 0.0f, 0.0f);
            return;
        }

        // --- A. HORIZONTAL MAX SENSOR RANGE THRESHOLD CHECK ---
        float dist = distance (currentPos, filterCenter);

        if (dist > filterRadius)
        {
            gl_Position = vec4 (0.0f, 0.0f, 0.0f, 0.0f); // Collapse vertex instantly
            return;
        }

        // --- B. PRODUCTION REPAIR: FLAT TANGENT HORIZON PLANE GUARD ---
        vec3 toSatellite = currentPos - filterCenter;
        vec3 planeNormal = normalize (filterCenter); // Upward surface normal vector

        // Corrected comparison mapping parameter path
        if (dot (toSatellite, planeNormal) < -0.0005f)
        {
            gl_Position = vec4 (0.0f, 0.0f, 0.0f, 0.0f); // Collapse vertex instantly
            return;
        }
    }

    float status = entities[gl_VertexID].velocity.w;

        
    // Map colors and sizes procedurally based on your specific status markers
    if (typeId == 4.0f)
    {
        vColor = vec4 (1.0f, 0.0f, 0.0f, 1.0f); // Bright Red = Incoming Ballistic Threat
        gl_PointSize = 10.0f;                   // Bold 12px target block at 4K
    } 
    else if (typeId == 3.0f)
    {
        vColor = vec4 (0.0f, 0.8f, 1.0f, 1.0f); // Electric Blue = Defensive Interceptor
        gl_PointSize = 8.0f;                   // Distinct 10px defense marker
    }
    else if (status > 0.5f)
    {
        vColor = satColor;                     // Magenta = Active Satellite (status == 1.0)
        gl_PointSize = 6.0f;                   // Crisp 8px orbit node
    } 
    else
    {
        vColor = vec4 (1.0f, 0.4f, 0.0f, 0.7f); // Orange Glow = Tumbling Debris Shard (status == 0.0)
        gl_PointSize = 4.0f;
    }

    // Apply projection transforms cleanly to send coordinates to the clip matrix
    gl_Position = mvp * vec4 (currentPos, 1.0f);
}

