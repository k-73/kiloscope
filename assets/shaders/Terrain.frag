// Terrain fragment shader — technical topographic visualization.
// Vertex data: .x = elevation (m), .y = slope (0=flat, 1=vertical).
#version 450 core

in vec3  vWorldPos;
in vec3  vNormal;
in vec4  vColor;
in float vLogZ;

uniform vec3  uLightDir;
uniform float uAmbient;
uniform vec3  uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uFcoefHalf;
uniform float uElevMin;
uniform float uElevMax;

out vec4 FragColor;

// Topographic color bands — stepped, not blended
vec3 elevColor(float t) {
    const vec3 c0 = vec3(0.12, 0.20, 0.17);  // deep low
    const vec3 c1 = vec3(0.16, 0.28, 0.15);  // lowland
    const vec3 c2 = vec3(0.24, 0.34, 0.18);  // plains
    const vec3 c3 = vec3(0.34, 0.38, 0.22);  // hills
    const vec3 c4 = vec3(0.46, 0.40, 0.26);  // upland
    const vec3 c5 = vec3(0.56, 0.46, 0.32);  // mountain
    const vec3 c6 = vec3(0.66, 0.62, 0.56);  // high rock
    const vec3 c7 = vec3(0.84, 0.82, 0.80);  // alpine
    const vec3 c8 = vec3(0.94, 0.94, 0.93);  // snow

    // Hard-step bands: each band has a flat color with a slight internal gradient
    if (t < 0.08) return mix(c0, c1, t / 0.08);
    if (t < 0.16) return mix(c1, c2, (t - 0.08) / 0.08);
    if (t < 0.28) return mix(c2, c3, (t - 0.16) / 0.12);
    if (t < 0.40) return mix(c3, c4, (t - 0.28) / 0.12);
    if (t < 0.55) return mix(c4, c5, (t - 0.40) / 0.15);
    if (t < 0.70) return mix(c5, c6, (t - 0.55) / 0.15);
    if (t < 0.85) return mix(c6, c7, (t - 0.70) / 0.15);
    return mix(c7, c8, (t - 0.85) / 0.15);
}

// Sharp contour line — pixel-width via fwidth, returns 0 or 1
float contour(float value, float interval, float thickness) {
    float d = abs(mod(value + interval * 0.5, interval) - interval * 0.5);
    float w = fwidth(value) * thickness;
    return 1.0 - smoothstep(0.0, w, d);
}

void main() {
    gl_FragDepth = log2(vLogZ) * uFcoefHalf;

    float elev  = vColor.x;
    float slope = vColor.y;
    float range = max(uElevMax - uElevMin, 1.0);
    float t     = clamp((elev - uElevMin) / range, 0.0, 1.0);

    // Base: quantized color bands (stepped look per 20m band)
    float bandElev = floor(elev / 20.0) * 20.0;
    float tBand    = clamp((bandElev - uElevMin) / range, 0.0, 1.0);
    vec3 base      = elevColor(tBand);

    // Slope emphasis — steep faces darker + slight warm tint
    float sf = 1.0 - slope * 0.55;
    base *= sf;

    // Contour lines — crisp, screen-space width via fwidth()
    float major = contour(elev, 100.0, 1.2);  // 100m — bold
    float minor = contour(elev, 20.0, 0.8);   // 20m  — thin

    // Distance-adaptive fade for minor lines (majors always visible)
    float dist = length(vWorldPos);
    float minorFade = 1.0 - smoothstep(5000.0, 15000.0, dist);

    // Apply: dark overlay
    base = mix(base, base * 0.25, major * 0.85);
    base = mix(base, base * 0.55, minor * minorFade * 0.50);

    // Lighting — directional + hemisphere, moderate contrast
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    float diff = max(dot(N, L), 0.0) * 0.6;
    float hemi = uAmbient + uAmbient * 0.2 * max(N.z, 0.0);
    vec3 lit = base * (hemi + diff);

    // Fog
    if (uFogStart > 0.0) {
        float fog = smoothstep(uFogStart, uFogEnd, dist);
        fog *= fog;
        lit = mix(lit, uFogColor, fog);
    }

    FragColor = vec4(lit, 1.0);
}
