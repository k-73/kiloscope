// Point pick fragment shader — circle clip + logarithmic depth.
#version 450 core

flat in uint vPickId;
in float vLogZ;

uniform float uFarPlane;

layout(location = 0) out uint FragId;

void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    if (dot(c, c) > 1.0) discard;
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = log2(vLogZ) * Fcoef_half;
    FragId = vPickId;
}
