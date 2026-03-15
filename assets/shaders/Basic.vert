// Mesh vertex shader — transforms pre-computed world-space vertices to clip space.
// Positions and normals arrive already in world space (CPU-side transform).
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uViewProj;  // combined view-projection matrix

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    vWorldPos  = aPos;
    vNormal    = aNormal;
    vTexCoord  = aTexCoord;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
