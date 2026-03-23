// Mesh vertex shader — transforms model-space vertices to clip space.
// Cached meshes arrive in unit/model space (GPU-side transform via uModel).
// Flat meshes arrive pre-transformed (uModel = identity).
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uViewProj;
uniform mat4 uModel;
uniform mat3 uNormalMat;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos  = worldPos.xyz;
    vNormal    = normalize(uNormalMat * aNormal);
    vTexCoord  = aTexCoord;
    gl_Position = uViewProj * worldPos;
}
