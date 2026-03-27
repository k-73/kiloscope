// Mesh pick vertex shader — logarithmic depth for consistency with visual pass.
#version 450 core

layout(location = 0) in vec3 aPos;

uniform mat4 uViewProj;
uniform mat4 uModel;
uniform float uFarPlane;

out float vLogZ;

void main() {
    gl_Position = uViewProj * uModel * vec4(aPos, 1.0);
    float Fcoef = 2.0 / log2(uFarPlane + 1.0);
    gl_Position.z = (log2(max(1e-6, gl_Position.w + 1.0)) * Fcoef - 1.0) * gl_Position.w;
    vLogZ = 1.0 + gl_Position.w;
}
