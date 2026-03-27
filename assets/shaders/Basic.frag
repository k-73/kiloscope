// Mesh fragment shader — PBR-inspired lighting with multiple shading modes.
// Logarithmic depth + atmospheric fog for large-scale scenes.
//
// Modes (uUnlit):
//   0 = Lit      — directional + point lights, wrap diffuse, GGX specular, fresnel rim
//   1 = Unlit    — flat color with fog
//   2 = Emissive — hot white core + bright rim glow
//   3 = Glow     — additive halo (rendered with GL_BLEND ONE/ONE)
#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in float vLogZ;

// ── per-draw uniforms ────────────────────────────────────────────────
uniform vec4 uColor;
uniform int  uUnlit;

// ── per-frame uniforms ───────────────────────────────────────────────
uniform vec3  uLightDir;
uniform vec3  uCamPos;
uniform vec3  uBgColor;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uRoughness;
uniform float uSpecular;
uniform float uFresnel;
uniform float uFogDensity;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uFarPlane;

// ── point lights ─────────────────────────────────────────────────────
#define MAX_POINT_LIGHTS 32
uniform int   uNumPointLights;
uniform vec3  uPLPos[MAX_POINT_LIGHTS];
uniform vec3  uPLColor[MAX_POINT_LIGHTS];
uniform float uPLRange[MAX_POINT_LIGHTS];

const float PI      = 3.14159265;
const float EPSILON = 0.001;

out vec4 FragColor;

// ── utility ──────────────────────────────────────────────────────────

float Fog(float d) {
    if (uFogStart > 0.0) {
        float t = clamp((d - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
        return 1.0 - t * t * (3.0 - 2.0 * t);
    }
    return clamp(exp(-uFogDensity * d * d), 0.0, 1.0);
}

float WrapDiffuse(float NdL) {
    float d = max(NdL * 0.5 + 0.5, 0.0);
    return d * d * uDiffuse;
}

float GGXSpec(float NdH, float r2) {
    float denom = NdH * NdH * (r2 - 1.0) + 1.0;
    return r2 / (PI * denom * denom + EPSILON) * uSpecular;
}

// ── main ─────────────────────────────────────────────────────────────

void main() {
    // Log depth
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = log2(vLogZ) * Fcoef_half;

    float dist = length(uCamPos - vWorldPos);
    float fog  = Fog(dist);
    vec3  fogColor = mix(uBgColor, vec3(0.55, 0.62, 0.75), 0.3);

    // Mode 1: flat color
    if (uUnlit == 1) {
        FragColor = vec4(mix(fogColor, uColor.rgb, fog), uColor.a);
        return;
    }

    // Mode 2: emissive
    if (uUnlit == 2) {
        vec3  N   = normalize(vNormal);
        vec3  V   = normalize(uCamPos - vWorldPos);
        float NdV = max(dot(N, V), 0.0);
        float rim = pow(1.0 - NdV, 2.5);
        vec3  hot = mix(uColor.rgb, vec3(1.0), NdV * 0.3);
        FragColor = vec4(mix(fogColor, hot * (1.0 + rim * 1.5), fog), uColor.a);
        return;
    }

    // Mode 3: additive glow
    if (uUnlit == 3) {
        vec3  N   = normalize(vNormal);
        vec3  V   = normalize(uCamPos - vWorldPos);
        float NdV = max(dot(N, V), 0.0);
        float intensity = NdV * NdV * NdV * NdV * uColor.a;
        FragColor = vec4(uColor.rgb * intensity, 0.0);
        return;
    }

    // ── Mode 0: lit shading ──────────────────────────────────────
    vec3  N   = normalize(vNormal);
    vec3  L   = normalize(uLightDir);
    vec3  V   = normalize(uCamPos - vWorldPos);
    vec3  H   = normalize(L + V);
    float NdL = dot(N, L);
    float NdV = dot(N, V);
    float r2  = uRoughness * uRoughness;

    float diff    = WrapDiffuse(NdL);
    float hemi    = N.z * 0.08 + uAmbient;
    float spec    = GGXSpec(max(dot(N, H), 0.0), r2);
    float fresnel = pow(1.0 - max(NdV, 0.0), 4.0) * uFresnel;

    vec3 lit = uColor.rgb * (hemi + diff + fresnel) + spec;

    for (int i = 0; i < min(uNumPointLights, MAX_POINT_LIGHTS); i++) {
        vec3  toLight = uPLPos[i] - vWorldPos;
        float d       = length(toLight);
        float atten   = clamp(1.0 - d / max(uPLRange[i], EPSILON), 0.0, 1.0);
        atten *= atten;
        vec3  PL    = normalize(toLight);
        float pDiff = WrapDiffuse(dot(N, PL));
        float pSpec = GGXSpec(max(dot(N, normalize(PL + V)), 0.0), r2);
        lit += (uColor.rgb * pDiff + pSpec) * uPLColor[i] * atten;
    }

    FragColor = vec4(mix(fogColor, lit, fog), uColor.a);
}
