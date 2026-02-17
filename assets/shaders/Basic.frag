#version 450 core
in vec3 vWorldPos;
in vec3 vNormal;

uniform vec4 uColor;
uniform vec3 uLightDir, uCamPos;
uniform int uUnlit;

out vec4 FragColor;

const vec3 FogColor = vec3(0.12, 0.12, 0.14);

float Fog(float d) { return clamp(exp(-0.00015 * d * d), 0.0, 1.0); }

void main() {
    float dist = length(uCamPos - vWorldPos);
    float fog = Fog(dist);

    if (uUnlit != 0) {
        FragColor = vec4(mix(FogColor, uColor.rgb, fog), uColor.a);
        return;
    }

    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0) * 0.65;
    float fill = max(-dot(N, L), 0.0) * 0.08;
    float spec = pow(max(dot(N, H), 0.0), 48.0) * 0.4;
    float rim  = pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.15;

    vec3 lit = uColor.rgb * (0.18 + diff + fill) + spec + vec3(0.6) * rim;
    FragColor = vec4(mix(FogColor, lit, fog), uColor.a);
}
