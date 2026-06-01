#version 460 core

struct GpuEntityData
{
    vec4 position;  // xyz = Current Position, w = Scale
    vec4 velocity;  // xyz = Core Move Direction, w = Target Speed (GL units/sec)
    vec4 metadata;  // x = Lifespan, y = Thrust, z = State ID, w = TYPE_ID
    vec4 padding;   
};

layout (std430, binding = 0) readonly buffer SimulationBlock
{
    GpuEntityData entities[];
};

// =========================================================================
// PRODUCTION REPAIR: EXPLICIT UNIFORM REGISTER BINDINGS
// =========================================================================
// Unified Hardcoded Uniform Locations matching Satellites.vert
layout (location = 0) uniform mat4 mvp;
layout (location = 3) uniform int tacticalStartSlot;
layout (location = 4) uniform vec3 filterCenter;
layout (location = 5) uniform float filterRadius;
layout (location = 6) uniform bool filterEnabled;

layout (location = 0) out float vFade;

void main()
{
    // 1. COMPUTE SYSTEM-LEVEL OFFSET
    // gl_InstanceID tracks which missile index loop the GPU is currently processing
    int currentMissileSlotIndex = tacticalStartSlot + gl_InstanceID;
    float typeId = entities[currentMissileSlotIndex].metadata.w;

    // Safety fallback gate
    if (typeId != 3.0f && typeId != 4.0f)
    {
        gl_Position = vec4 (0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    // Pull the targeted tracking metrics straight from your primary SSBO array
    vec3 currentPos = entities[currentMissileSlotIndex].position.xyz;
    vec3 moveDir    = normalize (entities[currentMissileSlotIndex].velocity.xyz); // Force normalization guard

    // Procedural line path spacing calculations (64 points deep)
    int localVertexIndex = gl_VertexID; 
    float segmentSpacing = 0.0035f; 
    float trailOffsetDistance = float (localVertexIndex) * segmentSpacing;

    // Project backward along the verified move direction vector
    vec3 vertexWorldPos = currentPos - (moveDir * trailOffsetDistance);
    
    // =========================================================================
    // UNIFIED INTERLOCK REPAIR: VISIBILITY FILTER GATES
    // =========================================================================
    if (filterEnabled)
    {
        // --- A. HORIZONTAL MAX SENSOR RANGE THRESHOLD CHECK ---
        float dist = distance (vertexWorldPos, filterCenter);

        if (dist > filterRadius)
        {
            gl_Position = vec4 (0.0f, 0.0f, 0.0f, 0.0f); // Collapse trail segment instantly
            return;
        }

        // --- B. FLAT TANGENT HORIZON PLANE GUARD ---
        vec3 toSegment = vertexWorldPos - filterCenter;
        vec3 planeNormal = normalize (filterCenter); // Upward surface normal vector
        
        if (dot (toSegment, planeNormal) < -0.0005f)
        {
            gl_Position = vec4 (0.0f, 0.0f, 0.0f, 0.0f); // Collapse trail segment instantly
            return;
        }
    }

    // Assign output values if the segment passes both visual filter checks
    vFade = float (63 - localVertexIndex) / 63.0f;
    gl_Position = mvp * vec4 (vertexWorldPos, 1.0f);
}
