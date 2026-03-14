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

// Shadow mapping
uniform sampler2DShadow uShadowMap;
uniform mat4 uLightVP;

// Point lights
#define MAX_POINT_LIGHTS 8
uniform int   uNumPointLights;
uniform vec3  uPLPos[MAX_POINT_LIGHTS];
uniform vec3  uPLColor[MAX_POINT_LIGHTS];
uniform float uPLRange[MAX_POINT_LIGHTS];

out vec4 FragColor;

float Fog(float d) { return clamp(exp(-uFogDensity * d * d), 0.0, 1.0); }

float Shadow(vec3 worldPos) {
    vec4 ls = uLightVP * vec4(worldPos, 1.0);
    vec3 proj = ls.xyz / ls.w * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;
    proj.z -= 0.002;
    return texture(uShadowMap, proj);
}

void main() {
    float dist = length(uCamPos - vWorldPos);
    float fog = Fog(dist);

    if (uUnlit != 0) {
        FragColor = vec4(mix(uBgColor, uColor.rgb, fog), uColor.a);
        return;
    }

    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);
    float NdL = dot(N, L);
    float NdV = dot(N, V);

    float shadow = Shadow(vWorldPos);

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

    // Directional light: shadow affects diffuse + specular, not ambient/fresnel
    vec3 lit = uColor.rgb * hemi
             + uColor.rgb * diff * shadow
             + vec3(1.0) * spec * shadow
             + uColor.rgb * fresnel;

    // Point lights
    for (int i = 0; i < uNumPointLights; i++) {
        vec3 toLight = uPLPos[i] - vWorldPos;
        float d = length(toLight);
        float atten = clamp(1.0 - d / uPLRange[i], 0.0, 1.0);
        atten *= atten;

        vec3 PL = normalize(toLight);
        float pDiff = max(dot(N, PL), 0.0) * uDiffuse;

        vec3 PH = normalize(PL + V);
        float pNdH = max(dot(N, PH), 0.0);
        float pDenom = pNdH * pNdH * (r2 - 1.0) + 1.0;
        float pSpec = r2 / (3.14159 * pDenom * pDenom + 0.001) * uSpecular;

        lit += (uColor.rgb * pDiff + vec3(1.0) * pSpec) * uPLColor[i] * atten;
    }

    FragColor = vec4(mix(uBgColor, lit, fog), uColor.a);
}
