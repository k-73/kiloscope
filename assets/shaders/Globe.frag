// WGS84 ellipsoid with multi-scale lat/lon graticule.
//
// Lat/lon via delta from camera (Cesium voxel approach):
//   Project ENU offset onto equatorial/meridional planes → atan on small vectors
//   → ~1000× better precision than atan on ~6.4M ECEF, no temporal jitter.
#version 450 core

in vec3 vNear;
in vec3 vDir;

uniform mat4  uViewProj;
uniform vec3  uCamPos;
uniform vec3  uEllCenter;      // camera-relative (for intersection)
uniform vec3  uRadii;
uniform mat3  uEcefToLocal;
uniform vec2  uCamLLAInt;      // floor(camera lat, lon)
uniform vec2  uCamLLAFracHi;  // fmod(camera lat/lon, 1°) — high float
uniform vec2  uCamLLAFracLo;  // fmod residual (double precision)
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

    // ── delta lat/lon from camera (Cesium approach) ──────────────
    // Use tFlat (flat plane) when available — NO dependency on uEllCenter → zero jitter.
    // Fallback to tHit for far-side-of-globe where tFlat is invalid.
    float tFlat = -(vNear.z + uCamPos.z) / ray.z;
    float tEnu  = (tFlat > 0.0) ? tFlat : tHit;
    vec3 enu = vNear + tEnu * ray;  // camera-relative ENU offset

    float cosL = uCamLat.x, sinL = uCamLat.y;
    float Nrad = uCurvature.x, Mrad = uCurvature.y;

    // Project onto equatorial plane → delta longitude
    vec2 eqCam = vec2((Nrad + uCamAlt) * cosL, 0.0);
    vec2 eqHit = eqCam + vec2(-enu.y * sinL + enu.z * cosL, enu.x);
    float dLon = degrees(atan(eqHit.y, eqHit.x));

    // Remove longitude component from ENU (versine: avoids cancellation)
    float sinHalfLon = sin(radians(dLon) * 0.5);
    float dx = length(eqHit) * 2.0 * sinHalfLon * sinHalfLon;
    vec3 enuCorr = enu + vec3(-enu.x, -dx * sinL, dx * cosL);

    // Project onto meridional plane → delta latitude
    vec2 merCam = vec2(Mrad + uCamAlt, 0.0);
    vec2 merHit = merCam + vec2(enuCorr.z, enuCorr.y);
    float dLat = degrees(atan(merHit.y, merHit.x));

    // Absolute lat/lon = camera (int + fracHi + delta + fracLo)
    // Fractional degrees only → ULP ≈ 1.2e-7° (30× finer than full degrees)
    vec2 ll = uCamLLAInt + vec2(
        (uCamLLAFracHi.y + dLon) + uCamLLAFracLo.y,
        (uCamLLAFracHi.x + dLat) + uCamLLAFracLo.x);

    // ── graticule ────────────────────────────────────────────────
    float dist = tHit;

    float line = max(
        max(max(gridLine(ll, 0.001) * 0.15 * smoothstep(5000.0,   500.0,   dist),
                gridLine(ll, 0.01)  * 0.25 * smoothstep(50000.0,  5000.0,  dist)),
                gridLine(ll, 0.1)   * 0.35 * smoothstep(200000.0, 20000.0, dist)),
        max(max(gridLine(ll, 1.0)   * 0.4,
                gridLine(ll, 5.0)   * 0.55),
            max(gridLine(ll, 10.0)  * 0.65,
                max(gridLine(ll, 30.0) * 0.8,
                    gridLine(ll, 90.0) * 1.0))));

    FragColor = vec4(mix(uSurfaceColor.rgb, uGratColor.rgb, clamp(line, 0.0, 1.0)),
                     uSurfaceColor.a);

    vec4 cp = uViewProj * vec4(vNear + tHit * ray + uCamPos, 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) / log2(uFarPlane + 1.0), 0.0, 1.0);
}
