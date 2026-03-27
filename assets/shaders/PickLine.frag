// Line pick fragment shader — logarithmic depth + pick ID.
#version 450 core

flat in uint vPickId;
in float vLogZ;

uniform float uFarPlane;

layout(location = 0) out uint FragId;

void main() {
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = log2(vLogZ) * Fcoef_half;
    FragId = vPickId;
}
