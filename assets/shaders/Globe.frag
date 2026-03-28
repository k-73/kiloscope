// WGS84 ellipsoid with multi-scale lat/lon graticule.
//
// Double-precision intersection eliminates the float32 qc=dot-1 cancellation
// that caused ~1m tHit error → jitter on fine grids.  With double, tHit has
// ~2nm precision, so tFlat (flat-plane workaround) is no longer needed and
// both Cesium-delta and ECEF grids share the same ellipsoid surface point.
#version 450 core

in vec3 vNear;
in vec3 vDir;

uniform mat4  uViewProj;
uniform vec3  uCamPos;
uniform dvec3 uEllCenterD;     // camera-relative earth center (double)
uniform dmat3 uEcefToLocalD;   // ECEF→ENU rotation (double)
uniform dvec3 uRadiiD;         // ellipsoid semi-axes (double)
uniform vec3  uRadii;          // ellipsoid semi-axes (float, for Bowring)
uniform vec2  uCamLLAInt;      // floor(camera lat, lon)
uniform vec2  uCamLLAFracHi;   // fmod(camera lat/lon, 1°) — high float
uniform vec2  uCamLLAFracLo;   // fmod residual
uniform float uCamAlt;         // camera altitude (m)
uniform vec2  uCamLat;         // cos(lat), sin(lat)
uniform vec2  uCurvature;      // N (prime vertical), M (meridional)
uniform vec4  uGratColor;
uniform vec4  uSurfaceColor;
uniform float uFarPlane;

out vec4 FragColor;

float gridLine(vec2 coord, float spacing) {
    vec2 c = coord / spacing;
    vec2 d = fwidth(c);
    float density = max(d.x, d.y);
    float vis = 1.0 - smoothstep(0.2, 0.5, density);
    vec2 a = abs(fract(c - 0.5) - 0.5) / d;
    return max(1.0 - smoothstep(0.3, 1.5, a.x),
               1.0 - smoothstep(0.3, 1.5, a.y)) * vis;
}

void main() {
    vec3 ray = normalize(vDir);

    // ── ellipsoid intersection (double precision) ────────────────
    // Float32 gives ~1m tHit error from qc=dot(oc,oc)-1 cancellation.
    // Double gives ~2nm — eliminates jitter, tFlat no longer needed.
    dmat3 localToEcefD = transpose(uEcefToLocalD);
    dvec3 ocD = (localToEcefD * (dvec3(vNear) - uEllCenterD)) / uRadiiD;
    dvec3 rdD = (localToEcefD * dvec3(ray)) / uRadiiD;

    double qa = dot(rdD, rdD);
    double qb = dot(ocD, rdD);
    double qc = dot(ocD, ocD) - 1.0lf;
    double disc = qb * qb - qa * qc;
    if (disc < 0.0lf) discard;

    double sd = sqrt(disc);
    double tD = qc / (-qb + sd);
    if (tD < 0.0lf) { tD = (-qb + sd) / qa; if (tD < 0.0lf) discard; }
    float tHit = float(tD);

    // ── delta lat/lon from camera (Cesium approach) ──────────────
    // tHit from double → stable, view-independent, same surface as ECEF.
    vec3 enu = vNear + tHit * ray;

    float cosL = uCamLat.x, sinL = uCamLat.y;
    float Nrad = uCurvature.x, Mrad = uCurvature.y;

    vec2 eqCam = vec2((Nrad + uCamAlt) * cosL, 0.0);
    vec2 eqHit = eqCam + vec2(-enu.y * sinL + enu.z * cosL, enu.x);
    float dLon = degrees(atan(eqHit.y, eqHit.x));

    float sinHalfLon = sin(radians(dLon) * 0.5);
    float dx = length(eqHit) * 2.0 * sinHalfLon * sinHalfLon;
    vec3 enuCorr = enu + vec3(-enu.x, -dx * sinL, dx * cosL);

    vec2 merCam = vec2(Mrad + uCamAlt, 0.0);
    vec2 merHit = merCam + vec2(enuCorr.z, enuCorr.y);
    float dLat = degrees(atan(merHit.y, merHit.x));

    vec2 ll = uCamLLAInt + vec2(
        (uCamLLAFracHi.y + dLon) + uCamLLAFracLo.y,
        (uCamLLAFracHi.x + dLat) + uCamLLAFracLo.x);

    // ── ECEF geodetic coordinates (full globe) ────────────────
    // Double subtraction (hit - center) avoids float32 cancellation at 6.4M.
    dvec3 hitD = dvec3(vNear) + tD * dvec3(ray);
    vec3 ecefHit = vec3(localToEcefD * (hitD - uEllCenterD));
    float p = length(ecefHit.xy);
    float theta = atan(ecefHit.z * uRadii.x, p * uRadii.z);
    float st = sin(theta), ct = cos(theta);
    float ab2 = uRadii.x * uRadii.x - uRadii.z * uRadii.z;
    vec2 llEcef = vec2(degrees(atan(ecefHit.y, ecefHit.x)),
                       degrees(atan(ecefHit.z + ab2 / uRadii.z * st*st*st,
                                    p - ab2 / uRadii.x * ct*ct*ct)));

    // ── graticule ────────────────────────────────────────────────
    // Single coordinate source (ll) for ALL grids → perfect alignment guaranteed.
    // ECEF (llEcef) only for the far hemisphere where Cesium delta wraps at ±180°.
    float dist = tHit;
    float angDist = max(abs(dLon), abs(dLat));
    bool farSide = angDist > 120.0;

    float fine = farSide ? 0.0 : max(
        max(gridLine(ll, 0.001) * 0.15 * smoothstep(5000.0,   500.0,   dist),
            gridLine(ll, 0.01)  * 0.25 * smoothstep(50000.0,  5000.0,  dist)),
            gridLine(ll, 0.1)   * 0.35 * smoothstep(200000.0, 20000.0, dist));

    float coarse = farSide
        ? max(max(gridLine(llEcef, 10.0) * 0.65,
                  max(gridLine(llEcef, 30.0) * 0.8,
                      gridLine(llEcef, 90.0) * 1.0)),
              max(gridLine(llEcef, 1.0)  * 0.4,
                  gridLine(llEcef, 5.0)  * 0.55))
        : max(max(gridLine(ll, 1.0)  * 0.4,
                  gridLine(ll, 5.0)  * 0.55),
              max(gridLine(ll, 10.0) * 0.65,
                  max(gridLine(ll, 30.0) * 0.8,
                      gridLine(ll, 90.0) * 1.0)));

    float line = clamp(max(fine, coarse), 0.0, 1.0);

    FragColor = vec4(mix(uSurfaceColor.rgb, uGratColor.rgb, clamp(line, 0.0, 1.0)),
                     uSurfaceColor.a);

    vec4 cp = uViewProj * vec4(vNear + tHit * ray + uCamPos, 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) / log2(uFarPlane + 1.0), 0.0, 1.0);
}
