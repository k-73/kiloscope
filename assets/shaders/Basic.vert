// Mesh vertex shader — transforms model-space vertices to clip space.
// Uses logarithmic depth for large-scale scenes.
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uViewProj;
uniform mat4 uModel;
uniform mat3 uNormalMat;
uniform float uFarPlane;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out float vLogZ;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos  = worldPos.xyz;
    vNormal    = normalize(uNormalMat * aNormal);
    vTexCoord  = aTexCoord;
    gl_Position = uViewProj * worldPos;

    // Logarithmic depth (Outerra method)
    float Fcoef = 2.0 / log2(uFarPlane + 1.0);
    gl_Position.z = (log2(max(1e-6, gl_Position.w + 1.0)) * Fcoef - 1.0) * gl_Position.w;
    vLogZ = 1.0 + gl_Position.w;
}
