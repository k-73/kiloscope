// Terrain fragment shader — lit per-vertex color with fog, logarithmic depth.
#version 450 core

in vec3  vWorldPos;
in vec3  vNormal;
in vec4  vColor;
in float vLogZ;

uniform vec3  uLightDir;
uniform vec3  uCamPos;
uniform float uAmbient;
uniform vec3  uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uFcoefHalf;   // 1.0 / log2(farPlane + 1.0)

out vec4 FragColor;

void main() {
    // Depth bias: terrain slightly closer than Globe (Globe uses +1e-4 bias away from camera)
    gl_FragDepth = log2(vLogZ) * uFcoefHalf - 1e-4;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);

    // Two-sided lighting (terrain normals may point inward depending on winding)
    float NdL = dot(N, L);
    float diff = max(abs(NdL), 0.0) * 0.7;
    float hemi = uAmbient + uAmbient * 0.3 * max(abs(N.z), 0.0);
    vec3 lit = vColor.rgb * (hemi + diff);

    // Distance fog (matches Globe fog profile)
    float dist = length(uCamPos - vWorldPos);
    if (uFogStart > 0.0) {
        float fog = smoothstep(uFogStart, uFogEnd, dist);
        fog *= fog;
        lit = mix(lit, uFogColor, fog);
    }

    FragColor = vec4(lit, vColor.a);
}
