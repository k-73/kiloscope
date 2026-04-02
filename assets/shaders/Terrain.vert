// Terrain vertex shader — transforms ECEF-relative positions to clip space.
//
// Vertex positions stored relative to mesh ECEF center (preserves float32 precision).
// Transform: ECEF-relative → ECEF-relative-to-origin → ENU → camera-relative → clip.
#version 450 core

layout(location = 0) in vec3 aRelPos;    // ECEF position relative to mesh center
layout(location = 1) in vec3 aNormal;    // ECEF-space normal (outward-facing)
layout(location = 2) in vec4 aColor;     // .x = elevation (m), .y = slope (0..1)

uniform mat3  uEcefToEnu;    // ECEF → ENU rotation at current GeoRef origin
uniform vec3  uMeshOffset;   // mesh ECEF center − GeoRef ECEF origin (float32, nearby)
uniform vec3  uCamEnu;       // camera position in ENU (for camera-relative rendering)
uniform mat4  uViewProj;
uniform float uFcoef;        // 2.0 / log2(farPlane + 1.0)

out vec3  vWorldPos;   // camera-relative ENU (for fog distance)
out vec3  vNormal;     // ENU-space normal (for lighting)
out vec4  vColor;      // .x = elevation, .y = slope (passed to fragment)
out float vLogZ;

void main() {
    vec3 enu    = uEcefToEnu * (aRelPos + uMeshOffset);
    vec3 camRel = enu - uCamEnu;

    vWorldPos = camRel;
    vNormal   = normalize(uEcefToEnu * aNormal);
    vColor    = aColor;

    gl_Position = uViewProj * vec4(camRel, 1.0);

    // Logarithmic depth (Outerra method, matches Basic.vert / Globe.frag)
    gl_Position.z = (log2(max(1e-6, gl_Position.w + 1.0)) * uFcoef - 1.0) * gl_Position.w;
    vLogZ = 1.0 + gl_Position.w;
}
