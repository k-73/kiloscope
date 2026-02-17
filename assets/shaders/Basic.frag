#version 450 core
in vec3 vWorldPos;
in vec3 vNormal;

uniform vec4 uColor;
uniform vec3 uLightDir, uCamPos;
uniform int uUnlit;

out vec4 FragColor;

const vec3 BgColor = vec3(0.12, 0.12, 0.14);

float Fog(float d) { return clamp(exp(-0.00015 * d * d), 0.0, 1.0); }

void main() {
    float dist = length(uCamPos - vWorldPos);
    float fog = Fog(dist);

    if (uUnlit != 0) {
        FragColor = vec4(mix(BgColor, uColor.rgb, fog), uColor.a);
        return;
    }

    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);
    float NdL = dot(N, L);
    float NdV = dot(N, V);

    // Wrap diffuse — softer transition into shadow
    float diff = max(NdL * 0.5 + 0.5, 0.0);
    diff = diff * diff * 0.7;

    // Subtle hemisphere ambient (sky + ground bounce)
    float hemi = N.z * 0.08 + 0.22;

    // GGX-like specular — wider, softer highlight
    float rough = 0.35;
    float r2 = rough * rough;
    float NdH = max(dot(N, H), 0.0);
    float denom = NdH * NdH * (r2 - 1.0) + 1.0;
    float spec = r2 / (3.14159 * denom * denom + 0.001) * 0.15;

    // Fresnel rim — brighter at grazing angles
    float fresnel = pow(1.0 - max(NdV, 0.0), 4.0) * 0.25;

    vec3 lit = uColor.rgb * (hemi + diff) + vec3(1.0) * spec + uColor.rgb * fresnel;

    FragColor = vec4(mix(BgColor, lit, fog), uColor.a);
}
