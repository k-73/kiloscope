// Terrain fragment shader — lit per-vertex color with fog + logarithmic depth.
// Normals and light direction are in ENU space. Fog uses camera-relative distance.
#version 450 core

in vec3  vWorldPos;   // camera-relative ENU
in vec3  vNormal;     // ENU-space normal
in vec4  vColor;
in float vLogZ;

uniform vec3  uLightDir;    // ENU-space light direction (normalized)
uniform float uAmbient;
uniform vec3  uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uFcoefHalf;   // 1.0 / log2(farPlane + 1.0)

out vec4 FragColor;

void main() {
    // Terrain wins over Globe (Globe writes depth with +1e-4 bias)
    gl_FragDepth = log2(vLogZ) * uFcoefHalf;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);

    float diff = max(dot(N, L), 0.0) * 0.7;
    float hemi = uAmbient + uAmbient * 0.3 * max(N.z, 0.0);  // hemisphere sky boost (Z = up in ENU)
    vec3 lit = vColor.rgb * (hemi + diff);

    // Distance fog (camera at origin in camera-relative coords)
    float dist = length(vWorldPos);
    if (uFogStart > 0.0) {
        float fog = smoothstep(uFogStart, uFogEnd, dist);
        fog *= fog;
        lit = mix(lit, uFogColor, fog);
    }

    FragColor = vec4(lit, vColor.a);
}
