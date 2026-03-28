// WGS84 ellipsoid — ray-cast surface with multi-scale lat/lon graticule.
//
// Single coordinate source: ENU hit on ellipsoid surface → delta lat/lon.
// Precise (small ENU values), correct (follows ellipsoid curvature),
// no two-system alignment issues. Origin hi/lo split for fine grid precision.
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

float gridLine(vec2 coord, float spacing) {
    vec2 c = coord / spacing;
    vec2 d = fwidth(c);
    vec2 a = abs(fract(c - 0.5) - 0.5) / d;
    return max(1.0 - smoothstep(0.3, 1.5, a.x),
               1.0 - smoothstep(0.3, 1.5, a.y));
}

void main() {
    vec3 ray = normalize(vDir);

    // ── ellipsoid intersection ───────────────────────────────────
    mat3 localToEcef = transpose(uEcefToLocal);
    vec3 ocN = (localToEcef * (vNear - uEllCenter)) / uRadii;
    vec3 rdN = (localToEcef * ray) / uRadii;

    float a = dot(rdN, rdN);
    float b = dot(ocN, rdN);
    float c = dot(ocN, ocN) - 1.0;
    float disc = b * b - a * c;
    if (disc < 0.0) discard;

    float sd = sqrt(disc);
    float tHit = c / (-b + sd);
    if (tHit < 0.0) { tHit = (-b + sd) / a; if (tHit < 0.0) discard; }

    // ── ENU hit on ellipsoid → lat/lon ───────────────────────────
    // vNear + tHit*ray = camera-relative hit on ellipsoid
    // + uCamPos = ENU relative to origin (small → precise)
    vec3  enu    = vNear + tHit * ray + uCamPos;
    float cosLat = cos(radians(uOriginInt.x + uOriginFracHi.x));
    float dLon   = degrees(enu.x / (uR * cosLat));
    float dLat   = degrees(enu.y / uR);

    // Fine grids: fractional degrees (hi + delta + lo → double precision)
    vec2 fineLL = vec2((uOriginFracHi.y + dLon) + uOriginFracLo.y,
                       (uOriginFracHi.x + dLat) + uOriginFracLo.x);
    // Coarse grids: full degrees
    vec2 fullLL = uOriginInt + fineLL;

    // ── graticule ────────────────────────────────────────────────
    float dist = tHit;

    float line = max(
        // Fine (fineLL — fractional degrees, hi/lo precision)
        max(gridLine(fineLL, 0.001) * 0.15 * smoothstep(5000.0,   500.0,   dist),
            gridLine(fineLL, 0.01)  * 0.25 * smoothstep(50000.0,  5000.0,  dist)),
        max(
        // Medium (fineLL for 0.1°, fullLL for 1°+)
        max(gridLine(fineLL, 0.1)   * 0.35 * smoothstep(500000.0,  50000.0,  dist),
            gridLine(fullLL, 1.0)   * 0.45 * smoothstep(2000000.0, 500000.0, dist)),
        // Coarse (fullLL — full degrees)
        max(max(gridLine(fullLL, 5.0)  * 0.55 * smoothstep(5000000.0, 2000000.0, dist),
                gridLine(fullLL, 10.0) * 0.65),
            max(gridLine(fullLL, 30.0) * 0.8,
                gridLine(fullLL, 90.0) * 1.0))));

    // ── output ───────────────────────────────────────────────────
    FragColor = vec4(mix(uSurfaceColor.rgb, uGratColor.rgb, clamp(line, 0.0, 1.0)),
                     uSurfaceColor.a);

    vec4 cp = uViewProj * vec4(vNear + tHit * ray + uCamPos, 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) / log2(uFarPlane + 1.0), 0.0, 1.0);
}
