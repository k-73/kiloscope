// WGS84 ellipsoid surface + metric grid + lat/lon graticule.
#version 450 core

in vec3 vNear;
in vec3 vDir;

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

// Identical to Grid.frag SoftLine — no modifications, proven to work.
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

    // ── Ray-ellipsoid intersection ───────────────────────────────
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

    // ── Surface hit point ────────────────────────────────────────
    vec3 hitLocal = ro + rd * tEll;
    vec3 hitWorld = vNear + tEll * rd;

    // ── Metric grid on ellipsoid surface (ENU meters) ────────────
    // hitWorld.xy = East/North position in ENU — correct for metric grid
    float m1 = SoftLine(hitWorld.xy, 10.0)    * 0.25;
    float m2 = SoftLine(hitWorld.xy, 100.0)   * 0.4;
    float m3 = SoftLine(hitWorld.xy, 1000.0)  * 0.6;
    float m4 = SoftLine(hitWorld.xy, 10000.0) * 0.8;
    float metricLine = max(max(m1, m2), max(m3, m4));

    // ── Lat/lon graticule ────────────────────────────────────────
    vec3 ecef = toEcef * (hitLocal - uEllCenter);
    float lon = degrees(atan(ecef.y, ecef.x));
    float lat = degrees(atan(ecef.z, length(ecef.xy)));
    vec2 ll = vec2(lon, lat);

    float g1 = SoftLine(ll, 1.0)  * 0.4;       // 1° ≈ 111km
    float g2 = SoftLine(ll, 10.0) * 0.6;        // 10°
    float g3 = SoftLine(ll, 90.0) * 0.9;        // equator/poles
    float geoLine = max(max(g1, g2), g3);

    // ── Compose ──────────────────────────────────────────────────
    float line = clamp(max(metricLine, geoLine), 0.0, 1.0);
    vec3 col = mix(uSurfaceColor.rgb, uGratColor.rgb, line);

    FragColor = vec4(col, uSurfaceColor.a);

    vec4 cp = uViewProj * vec4(hitWorld, 1.0);
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) * Fcoef_half, 0.0, 1.0);
}
