// WGS84 ellipsoid — surface from ray-ellipsoid, metric grid from ray-plane (like Grid.frag).
// The flat plane at Z=0 matches the ellipsoid surface near the reference point.
// This ensures fwidth() works correctly for grid antialiasing.
#version 450 core

in vec3 vNear, vFar;

uniform mat4  uViewProj;
uniform vec3  uCamPos;
uniform vec3  uEllCenter;
uniform vec3  uRadii;
uniform mat3  uEcefToLocal;
uniform vec4  uGratColor;
uniform vec4  uSurfaceColor;
uniform float uFarPlane;

out vec4 FragColor;

float GridLine(vec2 coord, float scale) {
    vec2 c = coord / scale;
    vec2 d = fwidth(c);
    d = max(d, vec2(1e-4));
    vec2 a = abs(fract(c - 0.5) - 0.5) / d;
    return max(1.0 - smoothstep(0.3, 1.5, a.x),
               1.0 - smoothstep(0.3, 1.5, a.y));
}

void main() {
    vec3 rayVec = vFar - vNear;
    vec3 rd = normalize(rayVec);
    vec3 ro = vNear - uCamPos;

    // ── Ray-ellipsoid intersection (for surface detection + depth) ──
    mat3 toEcef = transpose(uEcefToLocal);
    vec3 oc_ecef = toEcef * (ro - uEllCenter);
    vec3 rd_ecef = toEcef * rd;
    vec3 oc_n = oc_ecef / uRadii;
    vec3 rd_n = rd_ecef / uRadii;

    float A    = dot(rd_n, rd_n);
    float B    = dot(oc_n, rd_n);
    float C    = dot(oc_n, oc_n) - 1.0;
    float disc = B * B - A * C;
    if (disc < 0.0) discard;

    float tEll = (-B - sqrt(disc)) / A;
    if (tEll < 1.0) discard;

    // Ellipsoid hit point (for depth + lat/lon)
    vec3 hitLocal = ro + rd * tEll;
    float tNorm = tEll / length(rayVec);
    vec3 hitWorld = vNear + tNorm * rayVec;

    // ── Metric grid from flat plane Z=0 (same method as Grid.frag) ──
    // This gives perfect fwidth() because it uses linear interpolation
    float tFlat = -vNear.z / rayVec.z;
    vec3 fp = vNear + tFlat * rayVec;
    vec2 meters = fp.xy;

    float g1 = GridLine(meters, 10.0)    * 0.3;
    float g2 = GridLine(meters, 100.0)   * 0.5;
    float g3 = GridLine(meters, 1000.0)  * 0.7;
    float g4 = GridLine(meters, 10000.0) * 0.9;

    // ── Lat/lon graticule ────────────────────────────────────────
    vec3 ecef = toEcef * (hitLocal - uEllCenter);
    float lon = degrees(atan(ecef.y, ecef.x));
    float lat = degrees(atan(ecef.z, length(ecef.xy)));
    float gl1 = GridLine(vec2(lon, lat), 1.0)  * 0.5;
    float gl2 = GridLine(vec2(lon, lat), 10.0) * 0.7;
    float gl3 = GridLine(vec2(lon, lat), 90.0);

    // ── Compose ──────────────────────────────────────────────────
    float line = max(max(g1, g2), max(g3, g4));
    line = max(line, max(max(gl1, gl2), gl3));

    vec3 col = mix(uSurfaceColor.rgb, uGratColor.rgb, clamp(line, 0.0, 1.0));

    FragColor    = vec4(col, uSurfaceColor.a);
    vec4 cp      = uViewProj * vec4(hitWorld, 1.0);
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) * Fcoef_half, 0.0, 1.0);
}
