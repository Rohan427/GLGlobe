#version 460 core

layout (location = 0) in vec3 pos; // Satellite world position

layout (location = 0) uniform mat4 mvp;
layout (location = 4) uniform vec3 filterCenter;
layout (location = 5) uniform float filterRadius;
layout (location = 6) uniform bool filterEnabled;

void main()
{
    // Is a Sensor enabled?
    if (filterEnabled)
    {
        float d = distance (pos, filterCenter);

        // Radius range - drop anything not in detection range
        if (d > filterRadius)
        {
            // Push the vertex outside the clipping volume (invisible)
            gl_Position = vec4 (2.0, 2.0, 2.0, 1.0);
            return;
        }

        // Tangent Plane Clipping (The "Horizon" filter)
        // Normal of the plane is the vector from center globe center (0,0,0) to filterCenter
        vec3 normal = normalize (filterCenter);
        
        // Calculate the signed distance of the satellite from the tangent plane
        // dot(S - P, n) > 0 means it's above the plane
        vec3 vToSat = pos - filterCenter;

        // Below tangent - drop it
        if (dot (vToSat, normal) < 0.0)
        {
            gl_Position = vec4 (2.0, 2.0, 2.0, 1.0);
            return;
        }
    }

    gl_Position = mvp * vec4 (pos, 1.0);
    // Control point size directly in the shader
    gl_PointSize = 8.0; 
}
