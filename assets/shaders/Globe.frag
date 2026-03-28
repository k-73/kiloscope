// WGS84 ellipsoid with multi-scale lat/lon graticule.
//
// Fine grids (< 1°): flat-plane ENU → stable (no uEllCenter dependency).
//                    Fade before flat-plane error grows.
// Coarse grids (≥ 1°): ECEF atan2 → correct on full globe.
//                       Self-cancelling: uEllCenter shift cancels in hit-center subtraction.
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

    float qa = dot(rdN, rdN);
    float qb = dot(ocN, rdN);
    float qc = dot(ocN, ocN) - 1.0;
    float disc = qb * qb - qa * qc;
    if (disc < 0.0) discard;

    float sd = sqrt(disc);
    float tHit = qc / (-qb + sd);
    if (tHit < 0.0) { tHit = (-qb + sd) / qa; if (tHit < 0.0) discard; }

    float dist = tHit;

    // ── FINE grids: flat-plane Z=0 (stable) ──────────────────────
    // tFlat has ZERO dependency on uEllCenter → no float32 quantization.
    // Fine layers fade before flat-plane approximation error grows.
    float tFlat  = max(-(vNear.z + uCamPos.z) / ray.z, 0.0);
    vec3  enuF   = vNear + tFlat * ray + uCamPos;
    float cosLat = cos(radians(uOriginInt.x + uOriginFracHi.x));

    vec2 fineLL = vec2(
        (uOriginFracHi.y + degrees(enuF.x / (uR * cosLat))) + uOriginFracLo.y,
        (uOriginFracHi.x + degrees(enuF.y / uR))            + uOriginFracLo.x);

    float fine = max(
        max(gridLine(fineLL, 0.001) * 0.15 * smoothstep(5000.0,   500.0,   dist),
            gridLine(fineLL, 0.01)  * 0.25 * smoothstep(50000.0,  5000.0,  dist)),
            gridLine(fineLL, 0.1)   * 0.35 * smoothstep(200000.0, 20000.0, dist));

    // ── COARSE grids: ECEF (correct on full globe) ───────────────
    // Self-cancelling: when uEllCenter shifts by ULP, the intersection
    // shifts by the same amount → subtraction cancels → lat/lon stable.
    vec3 ecef = localToEcef * (vNear + tHit * ray - uEllCenter);
    vec2 coarseLL = vec2(degrees(atan(ecef.y, ecef.x)),
                         degrees(atan(ecef.z, length(ecef.xy))));

    float coarse = max(
        max(gridLine(coarseLL, 1.0)  * 0.4,
            gridLine(coarseLL, 5.0)  * 0.55),
        max(gridLine(coarseLL, 10.0) * 0.65,
            max(gridLine(coarseLL, 30.0) * 0.8,
                gridLine(coarseLL, 90.0) * 1.0)));

    // ── compose ──────────────────────────────────────────────────
    float line = clamp(max(fine, coarse), 0.0, 1.0);
    FragColor = vec4(mix(uSurfaceColor.rgb, uGratColor.rgb, line), uSurfaceColor.a);

    // ── depth from ellipsoid ─────────────────────────────────────
    vec4 cp = uViewProj * vec4(vNear + tHit * ray + uCamPos, 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) / log2(uFarPlane + 1.0), 0.0, 1.0);
}
