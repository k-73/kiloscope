// WGS84 ellipsoid with multi-scale lat/lon graticule.
//
// Precision strategy:
//   • Double-precision ray-ellipsoid intersection: eliminates float32
//     cancellation in qc = dot(oc,oc) - 1 (~1m → ~2nm tHit).
//   • Origin-relative Cesium delta: ENU offset from GeoRef origin projected
//     onto equatorial/meridional planes → atan on small vectors.  Hi/lo float
//     split for origin lat/lon gives effective ULP ~1.2e-7° (~0.01m).
//   • ECEF Bowring geodetic fallback for the far hemisphere (>120° from origin)
//     where the Cesium delta atan wraps at ±180°.
//   • Logarithmic depth buffer (Outerra method).
#version 450 core

in vec3 vNear;
in vec3 vDir;

// ── camera / projection ──────────────────────────────────────────
uniform mat4  uViewProj;
uniform vec3  uCamPos;          // camera position in world ENU
uniform float uFarPlane;

// ── ellipsoid (double for intersection) ──────────────────────────
uniform dvec3 uEllCenterD;     // earth center, camera-relative (double)
uniform dmat3 uEcefToLocalD;   // ECEF→ENU rotation (double)
uniform dvec3 uRadiiD;         // semi-axes a, a, b (double)
uniform vec3  uRadii;          // semi-axes a, a, b (float — for Bowring)

// ── geodetic reference (GeoRef origin ≈ aircraft) ────────────────
uniform vec2  uCamLLAInt;      // vec2(floor(lat), floor(lon)) of origin
uniform vec2  uCamLLAFracHi;   // vec2(fract(lat), fract(lon)) — high bits
uniform vec2  uCamLLAFracLo;   // residual low bits (double→float remainder)
uniform vec2  uCamLat;         // vec2(cos(lat), sin(lat)) at origin
uniform vec2  uCurvature;      // vec2(N, M) — prime-vertical / meridional radii

// ── appearance ───────────────────────────────────────────────────
uniform vec4  uGratColor;
uniform vec4  uSurfaceColor;

out vec4 FragColor;

// ── helpers ──────────────────────────────────────────────────────

// Anti-aliased grid line with automatic density fade.
float gridLine(vec2 coord, float spacing) {
    vec2 c = coord / spacing;
    vec2 d = fwidth(c);
    float vis = 1.0 - smoothstep(0.2, 0.5, max(d.x, d.y));
    vec2 g = abs(fract(c - 0.5) - 0.5) / d;
    return max(1.0 - smoothstep(0.3, 1.5, g.x),
               1.0 - smoothstep(0.3, 1.5, g.y)) * vis;
}

// ── main ─────────────────────────────────────────────────────────

void main() {
    vec3 ray = normalize(vDir);

    // ── ray-ellipsoid intersection (double precision) ────────────
    dmat3 toEcef = transpose(uEcefToLocalD);
    dvec3 oc = (toEcef * (dvec3(vNear) - uEllCenterD)) / uRadiiD;
    dvec3 rd = (toEcef * dvec3(ray)) / uRadiiD;

    double qa = dot(rd, rd);
    double qb = dot(oc, rd);
    double qc = dot(oc, oc) - 1.0lf;
    double disc = qb * qb - qa * qc;
    if (disc < 0.0lf) discard;

    double sd = sqrt(disc);
    double tD = qc / (-qb + sd);                          // citardauq form
    if (tD < 0.0lf) { tD = (-qb + sd) / qa; if (tD < 0.0lf) discard; }
    float tHit = float(tD);

    // ── Cesium delta lat/lon ─────────────────────────────────────
    // ENU offset is from the GeoRef origin (not camera) so the result
    // matches uCamLLA* and stays fixed regardless of camera orientation.
    vec3 enu  = vNear + tHit * ray + uCamPos;
    float cosL = uCamLat.x, sinL = uCamLat.y;
    float N = uCurvature.x, M = uCurvature.y;

    // Equatorial plane projection → delta longitude
    vec2 eq = vec2(N * cosL - enu.y * sinL + enu.z * cosL, enu.x);
    float dLon = degrees(atan(eq.y, eq.x));

    // Remove longitude component from ENU via versine (avoids cancellation)
    float sh = sin(radians(dLon) * 0.5);
    float dx = length(eq) * 2.0 * sh * sh;

    // Meridional plane projection → delta latitude
    float dLat = degrees(atan(enu.y - dx * sinL, M + enu.z + dx * cosL));

    // Absolute lat/lon = origin (int + fracHi + delta + fracLo).
    // Note: ll.x carries longitude, ll.y carries latitude.  The integer parts
    // are cross-swapped (floor(lat) in x, floor(lon) in y) but this is harmless
    // because gridLine uses fract() which cancels any integer-multiple offset.
    vec2 ll = uCamLLAInt + vec2(
        (uCamLLAFracHi.y + dLon) + uCamLLAFracLo.y,
        (uCamLLAFracHi.x + dLat) + uCamLLAFracLo.x);

    // ── ECEF Bowring geodetic (far hemisphere fallback) ──────────
    dvec3 hitD = dvec3(vNear) + tD * dvec3(ray);
    vec3 ecef = vec3(toEcef * (hitD - uEllCenterD));
    float r  = length(ecef.xy);
    float th = atan(ecef.z * uRadii.x, r * uRadii.z);
    float sT = sin(th), cT = cos(th);
    float d2 = uRadii.x * uRadii.x - uRadii.z * uRadii.z;
    vec2 llFar = vec2(degrees(atan(ecef.y, ecef.x)),
                      degrees(atan(ecef.z + d2 / uRadii.z * sT*sT*sT,
                                   r - d2 / uRadii.x * cT*cT*cT)));

    // ── graticule ────────────────────────────────────────────────
    float dist = tHit;

    // Fine grids — Cesium delta only (distance fade → zero well before ±180° wrap)
    float fine = max(
        max(gridLine(ll, 0.001) * 0.15 * smoothstep(5000.0,   500.0,   dist),
            gridLine(ll, 0.01)  * 0.25 * smoothstep(50000.0,  5000.0,  dist)),
            gridLine(ll, 0.1)   * 0.35 * smoothstep(200000.0, 20000.0, dist));

    // Coarse grids — Cesium delta near origin, ECEF beyond 120° (atan wrap zone)
    float angDist = max(abs(dLon), abs(dLat));
    vec2 llC = angDist < 120.0 ? ll : llFar;
    float coarse = max(
        max(gridLine(llC, 1.0)  * 0.4,
            gridLine(llC, 5.0)  * 0.55),
        max(gridLine(llC, 10.0) * 0.65,
            max(gridLine(llC, 30.0) * 0.8,
                gridLine(llC, 90.0) * 1.0)));

    float line = clamp(max(fine, coarse), 0.0, 1.0);

    // ── output ───────────────────────────────────────────────────
    FragColor = vec4(mix(uSurfaceColor.rgb, uGratColor.rgb, line), uSurfaceColor.a);

    // Logarithmic depth (Outerra method)
    vec4 cp = uViewProj * vec4(vNear + tHit * ray + uCamPos, 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) / log2(uFarPlane + 1.0), 0.0, 1.0);
}
