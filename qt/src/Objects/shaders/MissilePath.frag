#version 460 core

layout (location = 0) in float vFade;

layout (location = 7) uniform vec4 trailColor;

layout (location = 0) out vec4 fragColor;

void main()
{
    vec2 circ = gl_PointCoord - vec2 (0.5);

    if (dot(circ, circ) > 0.25) discard;   // Nice round points

    fragColor = vec4 (trailColor.rgb, trailColor.a * vFade);
}
