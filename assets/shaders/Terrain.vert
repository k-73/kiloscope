// Terrain vertex shader — transforms ECEF-relative positions to clip space.
//
// Vertex positions stored relative to mesh ECEF center (float32 precision).
// Transform pipeline:
//   1. Add uMeshOffset to get position relative to current GeoRef origin (ECEF)
//   2. Rotate by uEcefToLocal to get local frame position (NED/ENU)
//   3. Subtract uCamLocal for camera-relative rendering
#version 450 core

layout(location = 0) in vec3 aRelPos;       // ECEF position relative to mesh center
layout(location = 1) in vec3 aNormal;        // ECEF-space normal
layout(location = 2) in vec4 aColor;         // elevation color

uniform mat3  uEcefToLocal;     // ECEF → local frame rotation
uniform vec3  uMeshOffset;      // ecefCenter - ecefRef (ECEF delta, float32)
uniform vec3  uCamLocal;        // camera position in local frame
uniform mat4  uViewProj;
uniform mat3  uNormalMat;       // ECEF → ENU rotation (for lighting)
uniform float uFcoef;

out vec3  vWorldPos;
out vec3  vNormal;
out vec4  vColor;
out float vLogZ;

void main() {
    // ECEF-relative → ECEF-relative-to-origin → local frame → camera-relative
    vec3 ecefFromOrigin = aRelPos + uMeshOffset;
    vec3 local = uEcefToLocal * ecefFromOrigin;
    vec3 camRel = local - uCamLocal;

    vWorldPos  = camRel;
    vNormal    = normalize(uNormalMat * aNormal);
    vColor     = aColor;

    gl_Position = uViewProj * vec4(camRel, 1.0);

    // Logarithmic depth (Outerra method)
    gl_Position.z = (log2(max(1e-6, gl_Position.w + 1.0)) * uFcoef - 1.0) * gl_Position.w;
    vLogZ = 1.0 + gl_Position.w;
}
