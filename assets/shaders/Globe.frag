// WGS84 ellipsoid surface + metric grid + lat/lon graticule.
// Metric grid uses ray-plane Z=0 (same as Grid.frag). Ellipsoid for clip + depth.
#version 450 core

in vec3 vNear;
in vec3 vDir;    // unnormalized ray direction from vertex shader

uniform mat4  uViewProj;
uniform vec3  uCamPos;
uniform vec3  uEllCenter;
uniform vec3  uRadii;
uniform mat3  uEcefToLocal;
uniform vec4  uGratColor;
uniform vec4  uSurfaceColor;
uniform float uFarPlane;
uniform float uCamDist;

out vec4 FragColor;

float SoftLine(vec2 coord, float scale) {
    vec2 c  = coord / scale;
    vec2 d  = fwidth(c);
    vec2 a  = abs(fract(c - 0.5) - 0.5) / d;
    return max(1.0 - smoothstep(0.3, 1.5, a.x),
               1.0 - smoothstep(0.3, 1.5, a.y));
}

void main() {
    vec3 rd = normalize(vDir);
    vec3 ro = vNear - uCamPos;

    // ── Ray-plane Z=0 (metric grid) ────────────────────────────────
    float tPlane = -vNear.z / rd.z;
    if (tPlane < 0.0) discard;
    vec3 fp = vNear + tPlane * rd;

    // Metric grid
    float g1  = SoftLine(fp.xy, 10.0);
    float g10 = SoftLine(fp.xy, 100.0);
    float g1k = SoftLine(fp.xy, 1000.0);
    float g10k = SoftLine(fp.xy, 10000.0);

    // Distance fade — HORIZONTAL distance
    float d     = length(fp.xy - uCamPos.xy);
    float scale = 1.0 + uCamDist;
    float fade  = 1.0 - smoothstep(20.0 * scale, 80.0 * scale, d);

    float metricLine = (g1 * 0.3 + g10 * 0.5 + g1k * 0.7 + g10k * 0.9) * fade;

    // ── Ray-ellipsoid (surface clip + depth) ─────────────────────
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
    if (tEll < 0.0) discard;

    // ── Lat/lon graticule (from ellipsoid hit) ───────────────────
    vec3 hitLocal = ro + rd * tEll;
    vec3 ecef = toEcef * (hitLocal - uEllCenter);
    float lon = degrees(atan(ecef.y, ecef.x));
    float lat = degrees(atan(ecef.z, length(ecef.xy)));
    float gl1 = SoftLine(vec2(lon, lat), 1.0)  * 0.5;
    float gl2 = SoftLine(vec2(lon, lat), 10.0) * 0.7;
    float gl3 = SoftLine(vec2(lon, lat), 90.0);
    float geoLine = max(max(gl1, gl2), gl3);

    // ── Compose ──────────────────────────────────────────────────
    float line = clamp(max(metricLine, geoLine), 0.0, 1.0);
    vec3 col = mix(uSurfaceColor.rgb, uGratColor.rgb, line);

    FragColor = vec4(col, uSurfaceColor.a);

    // Depth from ellipsoid (curved surface)
    vec3 hitWorld = vNear + tEll * rd;
    vec4 cp = uViewProj * vec4(hitWorld, 1.0);
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) * Fcoef_half, 0.0, 1.0);
}
