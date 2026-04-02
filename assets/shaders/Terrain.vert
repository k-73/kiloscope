// Terrain vertex shader — transforms ECEF-relative positions to clip space.
// Vertex data stored relative to mesh center (float32 precision preserved).
// uRefEcefHi = mesh center in camera-relative local coords (computed on CPU in double).
#version 450 core

layout(location = 0) in vec3 aRelPos;       // position relative to mesh ECEF center
layout(location = 1) in vec3 aNormal;        // ECEF-space normal
layout(location = 2) in vec4 aColor;         // elevation color

uniform mat3  uEcefToLocal;     // ECEF rotation → local frame (NED/ENU)
uniform vec3  uRefEcefHi;       // mesh center in camera-relative local coords
uniform mat4  uViewProj;
uniform mat3  uNormalMat;
uniform float uFcoef;           // 2.0 / log2(farPlane + 1.0)

out vec3  vWorldPos;
out vec3  vNormal;
out vec4  vColor;
out float vLogZ;

void main() {
    // Transform ECEF-relative position to local frame, add camera-relative center offset
    vec3 local = uEcefToLocal * aRelPos + uRefEcefHi;

    vWorldPos  = local;
    vNormal    = normalize(uNormalMat * aNormal);
    vColor     = aColor;

    gl_Position = uViewProj * vec4(local, 1.0);

    // Logarithmic depth (Outerra method)
    gl_Position.z = (log2(max(1e-6, gl_Position.w + 1.0)) * uFcoef - 1.0) * gl_Position.w;
    vLogZ = 1.0 + gl_Position.w;
}
