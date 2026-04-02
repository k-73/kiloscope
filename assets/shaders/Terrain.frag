// Terrain fragment shader — technical topographic visualization.
// Vertex data: .x = elevation (m), .y = slope (0=flat, 1=vertical).
// Global range for color ramp (consistent meaning), local range for adaptive contours.
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
uniform float uElevMin;     // global range (all loaded tiles)
uniform float uElevMax;
uniform float uLocalMin;    // local range (current mesh)
uniform float uLocalMax;

out vec4 FragColor;

// Topographic color bands
vec3 elevColor(float t) {
    const vec3 c0 = vec3(0.12, 0.20, 0.17);
    const vec3 c1 = vec3(0.16, 0.28, 0.15);
    const vec3 c2 = vec3(0.24, 0.34, 0.18);
    const vec3 c3 = vec3(0.34, 0.38, 0.22);
    const vec3 c4 = vec3(0.46, 0.40, 0.26);
    const vec3 c5 = vec3(0.56, 0.46, 0.32);
    const vec3 c6 = vec3(0.66, 0.62, 0.56);
    const vec3 c7 = vec3(0.84, 0.82, 0.80);
    const vec3 c8 = vec3(0.94, 0.94, 0.93);

    if (t < 0.08) return mix(c0, c1, t / 0.08);
    if (t < 0.16) return mix(c1, c2, (t - 0.08) / 0.08);
    if (t < 0.28) return mix(c2, c3, (t - 0.16) / 0.12);
    if (t < 0.40) return mix(c3, c4, (t - 0.28) / 0.12);
    if (t < 0.55) return mix(c4, c5, (t - 0.40) / 0.15);
    if (t < 0.70) return mix(c5, c6, (t - 0.55) / 0.15);
    if (t < 0.85) return mix(c6, c7, (t - 0.70) / 0.15);
    return mix(c7, c8, (t - 0.85) / 0.15);
}

float contour(float value, float interval, float thickness) {
    float d = abs(mod(value + interval * 0.5, interval) - interval * 0.5);
    float w = fwidth(value) * thickness;
    return 1.0 - smoothstep(0.0, w, d);
}

void main() {
    gl_FragDepth = log2(vLogZ) * uFcoefHalf;

    float elev  = vColor.x;
    float slope = vColor.y;

    // Color ramp from GLOBAL range — consistent color = consistent altitude meaning
    float globalRange = max(uElevMax - uElevMin, 1.0);
    float tGlobal     = clamp((elev - uElevMin) / globalRange, 0.0, 1.0);
    float bandElev    = floor(elev / 20.0) * 20.0;
    float tBand       = clamp((bandElev - uElevMin) / globalRange, 0.0, 1.0);
    vec3 base         = elevColor(tBand);

    // Local contrast boost — subtle brightness variation based on local normalized position
    float localRange = max(uLocalMax - uLocalMin, 1.0);
    float tLocal     = clamp((elev - uLocalMin) / localRange, 0.0, 1.0);
    base *= 0.85 + 0.30 * tLocal;  // brighten higher areas within view

    // Slope emphasis
    base *= 1.0 - slope * 0.55;

    // Adaptive contour intervals based on local elevation range
    float majorInt, minorInt;
    if      (localRange < 50.0)   { majorInt = 10.0;  minorInt = 2.0; }
    else if (localRange < 200.0)  { majorInt = 20.0;  minorInt = 5.0; }
    else if (localRange < 500.0)  { majorInt = 50.0;  minorInt = 10.0; }
    else if (localRange < 2000.0) { majorInt = 100.0; minorInt = 20.0; }
    else                          { majorInt = 500.0; minorInt = 100.0; }

    float major = contour(elev, majorInt, 1.2);
    float minor = contour(elev, minorInt, 0.8);

    float dist = length(vWorldPos);
    float minorFade = 1.0 - smoothstep(5000.0, 15000.0, dist);

    base = mix(base, base * 0.25, major * 0.85);
    base = mix(base, base * 0.55, minor * minorFade * 0.50);

    // Lighting
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
