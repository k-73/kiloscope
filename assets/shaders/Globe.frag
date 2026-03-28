// WGS84 ellipsoid surface + metric grid + lat/lon graticule.
#version 450 core

in vec3 vNear;
in vec3 vDir;

uniform mat4  uViewProj;
uniform vec3  uCamPos;
uniform vec3  uEllCenter;      // ellipsoid center (camera-relative, float)
uniform vec3  uRadii;
uniform mat3  uEcefToLocal;
uniform vec2  uOriginLL;      // origin lat, lon in degrees
uniform float uR;             // Earth radius (meters)
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
    vec3 ro = vNear;

    // ── Ellipsoid intersection ───────────────────────────────────
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

    // Numerically stable near-root: avoids catastrophic cancellation when camera near surface
    // Standard: t = (-B - sqrt(disc)) / A  ← cancels when B<0 and |B|≈sqrt(disc)
    // Stable:   t = C / (-B + sqrt(disc))  ← algebraically equivalent, no cancellation
    float sqrtDisc = sqrt(disc);
    float tEll = C / (-B + sqrtDisc);
    if (tEll < 0.0) {
        // Try far root (camera inside ellipsoid or looking away)
        tEll = (-B + sqrtDisc) / A;
        if (tEll < 0.0) discard;
    }

    // Distance from camera to surface hit (for fading fine layers)
    float surfDist = tEll;

    // ── Lat/lon from flat plane hit (stable — no ECEF rotation matrix noise) ──
    // Flat plane at world Z=0, same as Grid.frag. Numerically stable because
    // it uses only vNear/rd/uCamPos (no changing rotation matrix).
    float tPlane = -(vNear.z + uCamPos.z) / rd.z;
    float tGrid = clamp(tPlane, 0.0, tEll);  // don't exceed ellipsoid surface
    vec3 hitENU = vNear + tGrid * rd + uCamPos;  // world ENU relative to origin

    // ENU → delta lat/lon (linear, precise for small offsets)
    float cosLat = cos(radians(uOriginLL.x));
    float lon = uOriginLL.y + degrees(hitENU.x / (uR * cosLat));
    float lat = uOriginLL.x + degrees(hitENU.y / uR);
    vec2 ll = vec2(lon, lat);

    // Lat/lon graticule — all scales stable with GPU double precision ECEF
    float g001 = SoftLine(ll, 0.001) * 0.15 * smoothstep(5000.0,     500.0,     surfDist);  // ≈111m
    float g01  = SoftLine(ll, 0.01)  * 0.25 * smoothstep(50000.0,    5000.0,    surfDist);  // ≈1.1km
    float g1   = SoftLine(ll, 0.1)   * 0.35 * smoothstep(500000.0,   50000.0,   surfDist);  // ≈11km
    float g10  = SoftLine(ll, 1.0)   * 0.45 * smoothstep(2000000.0,  500000.0,  surfDist);  // ≈111km
    float g5   = SoftLine(ll, 5.0)   * 0.55 * smoothstep(5000000.0,  2000000.0, surfDist);  // ≈556km
    float gA   = SoftLine(ll, 10.0)  * 0.65;
    float gB   = SoftLine(ll, 30.0)  * 0.8;
    float gC   = SoftLine(ll, 90.0)  * 1.0;

    // ── Compose ──────────────────────────────────────────────────
    float line = max(max(max(g001, g01), max(g1, g10)),
                     max(max(g5, gA), max(gB, gC)));
    vec3 col = mix(uSurfaceColor.rgb, uGratColor.rgb, line);

    FragColor = vec4(col, uSurfaceColor.a);

    vec3 hitWorld = vNear + tEll * rd + uCamPos;
    vec4 cp = uViewProj * vec4(hitWorld, 1.0);
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) * Fcoef_half, 0.0, 1.0);
}
