// Mesh fragment shader — PBR-inspired lighting with multiple shading modes.
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

// ── per-draw uniforms ────────────────────────────────────────────────
uniform vec4 uColor;
uniform int  uUnlit;    // shading mode (see header)

// ── per-frame uniforms (directional light + environment) ─────────────
uniform vec3  uLightDir;   // directional light direction (world space)
uniform vec3  uCamPos;     // camera position (world space)
uniform vec3  uBgColor;    // background / fog color
uniform float uAmbient;
uniform float uDiffuse;
uniform float uRoughness;
uniform float uSpecular;
uniform float uFresnel;
uniform float uFogDensity;

// ── point lights (must match kMaxPointLights in DrawState.hpp) ───────
#define MAX_POINT_LIGHTS 32
uniform int   uNumPointLights;
uniform vec3  uPLPos[MAX_POINT_LIGHTS];
uniform vec3  uPLColor[MAX_POINT_LIGHTS];
uniform float uPLRange[MAX_POINT_LIGHTS];

// ── constants ────────────────────────────────────────────────────────
const float PI      = 3.14159265;
const float EPSILON = 0.001;

out vec4 FragColor;

// ── utility functions ────────────────────────────────────────────────

// Quadratic fog — falls off with squared distance
float Fog(float d) {
    return clamp(exp(-uFogDensity * d * d), 0.0, 1.0);
}

// Wrap diffuse — remaps NdL [-1,1] → [0,1], squared for softer falloff
float WrapDiffuse(float NdL) {
    float d = max(NdL * 0.5 + 0.5, 0.0);
    return d * d * uDiffuse;
}

// GGX-inspired specular distribution (simplified single-lobe)
float GGXSpec(float NdH, float r2) {
    float denom = NdH * NdH * (r2 - 1.0) + 1.0;
    return r2 / (PI * denom * denom + EPSILON) * uSpecular;
}

// ── main ─────────────────────────────────────────────────────────────

void main() {
    float dist = length(uCamPos - vWorldPos);
    float fog  = Fog(dist);

    // Mode 1: flat color, no lighting
    if (uUnlit == 1) {
        FragColor = vec4(mix(uBgColor, uColor.rgb, fog), uColor.a);
        return;
    }

    // Mode 2: emissive — hot white core fading to colored rim
    if (uUnlit == 2) {
        vec3  N   = normalize(vNormal);
        vec3  V   = normalize(uCamPos - vWorldPos);
        float NdV = max(dot(N, V), 0.0);
        float rim = pow(1.0 - NdV, 2.5);                    // bright edge falloff
        vec3  hot = mix(uColor.rgb, vec3(1.0), NdV * 0.3);  // white-hot center
        FragColor = vec4(mix(uBgColor, hot * (1.0 + rim * 1.5), fog), uColor.a);
        return;
    }

    // Mode 3: additive glow — rendered with GL_BLEND(ONE, ONE)
    // Sharp falloff (pow 4): bright core, fast fade to edges
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

    // Directional light contribution
    float diff    = WrapDiffuse(NdL);
    float hemi    = N.z * 0.08 + uAmbient;  // hemisphere ambient (Z = up)
    float spec    = GGXSpec(max(dot(N, H), 0.0), r2);
    float fresnel = pow(1.0 - max(NdV, 0.0), 4.0) * uFresnel;  // Schlick (power 4)

    vec3 lit = uColor.rgb * (hemi + diff + fresnel) + spec;

    // Point light contributions (same shading as directional)
    for (int i = 0; i < min(uNumPointLights, MAX_POINT_LIGHTS); i++) {
        vec3  toLight = uPLPos[i] - vWorldPos;
        float d       = length(toLight);
        float atten   = clamp(1.0 - d / max(uPLRange[i], EPSILON), 0.0, 1.0);
        atten *= atten;  // quadratic falloff

        vec3  PL    = normalize(toLight);
        float pDiff = WrapDiffuse(dot(N, PL));
        float pSpec = GGXSpec(max(dot(N, normalize(PL + V)), 0.0), r2);

        lit += (uColor.rgb * pDiff + pSpec) * uPLColor[i] * atten;
    }

    FragColor = vec4(mix(uBgColor, lit, fog), uColor.a);
}
