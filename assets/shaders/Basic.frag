#version 450 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;

uniform vec4 uColor;
uniform vec3 uLightDir, uCamPos;
uniform int uUnlit;            // 0=lit, 1=unlit, 2=emissive, 3=glow

uniform vec3  uBgColor;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uRoughness;
uniform float uSpecular;
uniform float uFresnel;
uniform float uFogDensity;

// Point lights (must match kMaxPointLights in DrawState.hpp)
#define MAX_POINT_LIGHTS 32
uniform int   uNumPointLights;
uniform vec3  uPLPos[MAX_POINT_LIGHTS];
uniform vec3  uPLColor[MAX_POINT_LIGHTS];
uniform float uPLRange[MAX_POINT_LIGHTS];

const float PI = 3.14159265;
const float EPSILON = 0.001;

out vec4 FragColor;

// Quadratic fog: density falls off with distance squared
float Fog(float d) { return clamp(exp(-uFogDensity * d * d), 0.0, 1.0); }

// Wrap diffuse: remaps NdL from [-1,1] to [0,1], squared for softer falloff
float WrapDiffuse(float NdL) {
    float d = max(NdL * 0.5 + 0.5, 0.0);
    return d * d * uDiffuse;
}

// GGX-inspired specular distribution
float GGXSpec(float NdH, float r2) {
    float denom = NdH * NdH * (r2 - 1.0) + 1.0;
    return r2 / (PI * denom * denom + EPSILON) * uSpecular;
}

void main() {
    float dist = length(uCamPos - vWorldPos);
    float fog = Fog(dist);

    // Flat color, no lighting
    if (uUnlit == 1) {
        FragColor = vec4(mix(uBgColor, uColor.rgb, fog), uColor.a);
        return;
    }

    // Emissive: hot white core, bright rim for glow effect
    if (uUnlit == 2) {
        vec3 N = normalize(vNormal);
        vec3 V = normalize(uCamPos - vWorldPos);
        float NdV = max(dot(N, V), 0.0);
        float rim  = pow(1.0 - NdV, 2.5);
        vec3 hot   = mix(uColor.rgb, vec3(1.0), NdV * 0.3);
        vec3 glow  = hot * (1.0 + rim * 1.5);
        FragColor = vec4(mix(uBgColor, glow, fog), uColor.a);
        return;
    }

    // Additive glow: NdV-based falloff, alpha controls intensity
    if (uUnlit == 3) {
        vec3 N = normalize(vNormal);
        vec3 V = normalize(uCamPos - vWorldPos);
        float NdV = max(dot(N, V), 0.0);
        float intensity = pow(NdV, 2.0) * uColor.a;
        FragColor = vec4(uColor.rgb * intensity, 0.0);
        return;
    }

    // ── Lit shading ──────────────────────────────────────────────

    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);
    float NdL = dot(N, L);
    float NdV = dot(N, V);
    float r2  = uRoughness * uRoughness;

    float diff = WrapDiffuse(NdL);
    float hemi = N.z * 0.08 + uAmbient;       // hemisphere ambient (Z = up)
    float spec = GGXSpec(max(dot(N, H), 0.0), r2);

    // Schlick-like fresnel rim (power 4 instead of standard 5 for softer look)
    float fresnel = pow(1.0 - max(NdV, 0.0), 4.0) * uFresnel;

    vec3 lit = uColor.rgb * (hemi + diff + fresnel) + spec;

    // Point lights (same shading model as directional; min() guards against CPU overflow)
    for (int i = 0; i < min(uNumPointLights, MAX_POINT_LIGHTS); i++) {
        vec3 toLight = uPLPos[i] - vWorldPos;
        float d = length(toLight);
        float atten = clamp(1.0 - d / max(uPLRange[i], EPSILON), 0.0, 1.0);
        atten *= atten; // quadratic falloff

        vec3 PL = normalize(toLight);
        float pDiff = WrapDiffuse(dot(N, PL));
        float pSpec = GGXSpec(max(dot(N, normalize(PL + V)), 0.0), r2);

        lit += (uColor.rgb * pDiff + pSpec) * uPLColor[i] * atten;
    }

    FragColor = vec4(mix(uBgColor, lit, fog), uColor.a);
}
