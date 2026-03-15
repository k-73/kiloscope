#version 450 core
in vec3 vWorldPos;
in vec3 vNormal;

uniform vec4 uColor;
uniform vec3 uLightDir, uCamPos;
uniform int uUnlit;

uniform vec3  uBgColor;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uRoughness;
uniform float uSpecular;
uniform float uFresnel;
uniform float uFogDensity;

// Point lights
#define MAX_POINT_LIGHTS 32
uniform int   uNumPointLights;
uniform vec3  uPLPos[MAX_POINT_LIGHTS];
uniform vec3  uPLColor[MAX_POINT_LIGHTS];
uniform float uPLRange[MAX_POINT_LIGHTS];

out vec4 FragColor;

float Fog(float d) { return clamp(exp(-uFogDensity * d * d), 0.0, 1.0); }

void main() {
    float dist = length(uCamPos - vWorldPos);
    float fog = Fog(dist);

    if (uUnlit == 1) {
        FragColor = vec4(mix(uBgColor, uColor.rgb, fog), uColor.a);
        return;
    }

    if (uUnlit == 2) {
        vec3 N = normalize(vNormal);
        vec3 V = normalize(uCamPos - vWorldPos);
        float NdV = max(dot(N, V), 0.0);
        float core = NdV;
        float rim  = pow(1.0 - NdV, 2.5);
        vec3 hot   = mix(uColor.rgb, vec3(1.0), core * 0.3);
        vec3 glow  = hot * (1.0 + rim * 1.5);
        FragColor = vec4(mix(uBgColor, glow, fog), uColor.a);
        return;
    }

    if (uUnlit == 3) {
        vec3 N = normalize(vNormal);
        vec3 V = normalize(uCamPos - vWorldPos);
        float NdV = max(dot(N, V), 0.0);
        float intensity = pow(NdV, 2.0) * uColor.a;
        FragColor = vec4(uColor.rgb * intensity, 0.0);
        return;
    }

    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);
    float NdL = dot(N, L);
    float NdV = dot(N, V);

    // Wrap diffuse
    float diff = max(NdL * 0.5 + 0.5, 0.0);
    diff = diff * diff * uDiffuse;

    // Hemisphere ambient
    float hemi = N.z * 0.08 + uAmbient;

    // GGX-like specular
    float r2 = uRoughness * uRoughness;
    float NdH = max(dot(N, H), 0.0);
    float denom = NdH * NdH * (r2 - 1.0) + 1.0;
    float spec = r2 / (3.14159 * denom * denom + 0.001) * uSpecular;

    // Fresnel rim
    float fresnel = pow(1.0 - max(NdV, 0.0), 4.0) * uFresnel;

    vec3 lit = uColor.rgb * (hemi + diff + fresnel) + spec;

    // Point lights
    for (int i = 0; i < min(uNumPointLights, MAX_POINT_LIGHTS); i++) {
        vec3 toLight = uPLPos[i] - vWorldPos;
        float d = length(toLight);
        float atten = clamp(1.0 - d / max(uPLRange[i], 0.001), 0.0, 1.0);
        atten *= atten;

        vec3 PL = normalize(toLight);
        float pNdL = dot(N, PL);
        float pDiff = max(pNdL * 0.5 + 0.5, 0.0);
        pDiff = pDiff * pDiff * uDiffuse;

        vec3 PH = normalize(PL + V);
        float pNdH = max(dot(N, PH), 0.0);
        float pDenom = pNdH * pNdH * (r2 - 1.0) + 1.0;
        float pSpec = r2 / (3.14159 * pDenom * pDenom + 0.001) * uSpecular;

        lit += (uColor.rgb * pDiff + vec3(1.0) * pSpec) * uPLColor[i] * atten;
    }

    FragColor = vec4(mix(uBgColor, lit, fog), uColor.a);
}
