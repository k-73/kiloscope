// WGS84 ellipsoid — ray-cast surface with multi-scale lat/lon graticule.
//
// Fine grids (< 1°): flat-plane ENU → lat/lon (stable, no ECEF noise)
// Coarse grids (≥ 1°): ECEF → lat/lon (correct on curved surface at orbital view)
// Origin hi/lo split gives double-precision lat/lon for fine grids
#version 450 core

in vec3 vNear;
in vec3 vDir;

uniform mat4  uViewProj;
uniform vec3  uCamPos;
uniform vec3  uEllCenter;
uniform vec3  uRadii;
uniform mat3  uEcefToLocal;
uniform vec2  uOriginInt;
uniform vec2  uOriginFracHi;
uniform vec2  uOriginFracLo;
uniform float uR;
uniform vec4  uGratColor;
uniform vec4  uSurfaceColor;
uniform float uFarPlane;

out vec4 FragColor;

float SoftLine(vec2 coord, float scale) {
    vec2 c = coord / scale;
    vec2 d = fwidth(c);
    vec2 a = abs(fract(c - 0.5) - 0.5) / d;
    return max(1.0 - smoothstep(0.3, 1.5, a.x),
               1.0 - smoothstep(0.3, 1.5, a.y));
}

void main() {
    vec3 rd = normalize(vDir);

    // ── Ellipsoid intersection ───────────────────────────────────
    mat3 toEcef = transpose(uEcefToLocal);
    vec3 oc_n = (toEcef * (vNear - uEllCenter)) / uRadii;
    vec3 rd_n = (toEcef * rd) / uRadii;

    float A = dot(rd_n, rd_n);
    float B = dot(oc_n, rd_n);
    float C = dot(oc_n, oc_n) - 1.0;
    float disc = B * B - A * C;
    if (disc < 0.0) discard;

    float sqD = sqrt(disc);
    float tEll = C / (-B + sqD);
    if (tEll < 0.0) {
        tEll = (-B + sqD) / A;
        if (tEll < 0.0) discard;
    }

    float surfDist = tEll;

    // ── Fine lat/lon from flat plane (stable at close range) ─────
    float tPlane = -(vNear.z + uCamPos.z) / rd.z;
    vec3 hitENU = vNear + tPlane * rd + uCamPos;

    float cosLat = cos(radians(uOriginInt.x + uOriginFracHi.x));
    float dLon = degrees(hitENU.x / (uR * cosLat));
    float dLat = degrees(hitENU.y / uR);

    vec2 llFrac = vec2((uOriginFracHi.y + dLon) + uOriginFracLo.y,
                       (uOriginFracHi.x + dLat) + uOriginFracLo.x);

    // ── Coarse lat/lon from ECEF (correct on curved surface) ────
    vec3 ecef = toEcef * (vNear + tEll * rd - uEllCenter);
    float lonC = degrees(atan(ecef.y, ecef.x));
    float latC = degrees(atan(ecef.z, length(ecef.xy)));
    vec2 llCoarse = vec2(lonC, latC);

    // ── Graticule layers ─────────────────────────────────────────
    float line = max(
        // Fine (< 1°) — flat plane, hi/lo precision, fades with distance
        max(max(SoftLine(llFrac, 0.001) * 0.15 * smoothstep(5000.0,    500.0,    surfDist),
                SoftLine(llFrac, 0.01)  * 0.25 * smoothstep(50000.0,   5000.0,   surfDist)),
                SoftLine(llFrac, 0.1)   * 0.35 * smoothstep(500000.0,  50000.0,  surfDist)),
        // Coarse (≥ 1°) — ECEF, correct at orbital, fades at close range
        max(max(SoftLine(llCoarse, 1.0)  * 0.45 * smoothstep(2000000.0, 500000.0, surfDist),
                SoftLine(llCoarse, 5.0)  * 0.55 * smoothstep(5000000.0, 2000000.0, surfDist)),
            max(SoftLine(llCoarse, 10.0) * 0.65,
                max(SoftLine(llCoarse, 30.0) * 0.8,
                    SoftLine(llCoarse, 90.0) * 1.0))));

    FragColor = vec4(mix(uSurfaceColor.rgb, uGratColor.rgb, clamp(line, 0.0, 1.0)),
                     uSurfaceColor.a);

    // ── Depth from ellipsoid ─────────────────────────────────────
    vec4 cp = uViewProj * vec4(vNear + tEll * rd + uCamPos, 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) / log2(uFarPlane + 1.0), 0.0, 1.0);
}
