#version 460 core

layout (location = 0) out vec4 FragColor;

void main()
{
    // Basic circular point mask
    vec2 circ = gl_PointCoord - vec2 (0.5);
    if (dot (circ, circ) > 0.25) discard;
    
    FragColor = vec4 (1.0, 1.0, 1.0, 0.8); // White dots with slight alpha
}
