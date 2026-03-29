// WGS84 ellipsoid with multi-scale lat/lon graticule.
//
// Precision strategy:
//   • Double-precision ray-ellipsoid intersection (~2nm tHit).
//   • Origin-relative Cesium delta → atan on small vectors (~0.01m).
//   • ECEF Bowring geodetic fallback for the far hemisphere (>120°).
//   • Logarithmic depth buffer (Outerra method).
#version 450 core

in vec3 vNear;
in vec3 vDir;

// ── camera / projection ──────────────────────────────────────────
uniform mat4  uViewProj;
uniform vec3  uCamPos;
uniform float uFarPlane;

// ── ellipsoid (double for intersection) ──────────────────────────
uniform dvec3 uEllCenterD;
uniform dmat3 uToEcefNorm;      // transpose(ecefToEnu) * diag(1/radii) — precomputed
uniform dmat3 uEcefToLocalD;
uniform dvec3 uRadiiD;
uniform vec3  uRadii;

// ── geodetic reference (GeoRef origin ≈ aircraft) ────────────────
uniform vec2  uOriginLLInt;
uniform vec2  uOriginLLFracHi;
uniform vec2  uOriginLLFracLo;
uniform vec2  uOriginLat;       // vec2(cos(lat), sin(lat))
uniform vec2  uOriginNM;        // vec2(N, M)

// ── appearance ───────────────────────────────────────────────────
uniform vec4  uGratColor;
uniform vec4  uSurfaceColor;
uniform vec3  uLightDir;        // world-ENU light direction (normalized)
uniform float uAmbient;
uniform vec3  uAtmoColor;
uniform vec2  uAtmoParams;      // x = power, y = intensity
uniform vec4  uGridFades;       // max visible distance for 0.0001°, 0.001°, 0.01°, 0.1° grids

out vec4 FragColor;

// ── helpers ──────────────────────────────────────────────────────

float gridLine(vec2 coord, float spacing) {
    vec2 c = coord / spacing;
    vec2 d = fwidth(c);
    vec2 g = abs(fract(c - 0.5) - 0.5) / d;
    return max(1.0 - smoothstep(0.3, 1.2, g.x),
               1.0 - smoothstep(0.3, 1.2, g.y));
}

float distFade(float dist, float maxDist) {
    return 1.0 - smoothstep(maxDist * 0.15, maxDist, dist);
}

void main() {
    vec3 ray = normalize(vDir);

    // ── ray-ellipsoid intersection (double precision) ────────────
    // uToEcefNorm = transpose(ecefToEnu) * diag(1/radii), precomputed on CPU.
    dvec3 oc = uToEcefNorm * (dvec3(vNear) - uEllCenterD);
    dvec3 rd = uToEcefNorm * dvec3(ray);

    double qa = dot(rd, rd);
    double qb = dot(oc, rd);
    double qc = dot(oc, oc) - 1.0lf;
    double disc = qb * qb - qa * qc;
    if (disc < 0.0lf) discard;

    double sd = sqrt(disc);
    double tD = qc / (-qb + sd);
    if (tD < 0.0lf) { tD = (-qb + sd) / qa; if (tD < 0.0lf) discard; }
    float tHit = float(tD);

    // ── surface point and normal ─────────────────────────────────
    vec3 hitWorld = vNear + tHit * ray + uCamPos;              // world ENU (float)
    // Reuse normalized intersection point: pN = oc + tD*rd lies on the unit ellipsoid.
    // ecef = pN * radii, normal = normalize(pN / radii).  Saves a full dmat3*dvec3.
    dvec3 pN        = oc + tD * rd;
    vec3 ecef       = vec3(pN * uRadiiD);
    vec3 normalEcef = normalize(vec3(pN / uRadiiD));
    // Flip normal when camera is inside the ellipsoid (qc < 0) so lighting is correct from below.
    if (qc < 0.0lf) normalEcef = -normalEcef;
    vec3 normal     = vec3(mat3(uEcefToLocalD) * normalEcef);

    // ── Cesium delta lat/lon ─────────────────────────────────────
    vec3 enu  = hitWorld;
    float cosL = uOriginLat.x, sinL = uOriginLat.y;
    float N = uOriginNM.x, M = uOriginNM.y;

    vec2 eq = vec2(N * cosL - enu.y * sinL + enu.z * cosL, enu.x);
    float dLon = degrees(atan(eq.y, eq.x));

    float sh = sin(radians(dLon) * 0.5);
    float dx = length(eq) * 2.0 * sh * sh;

    float dLat = degrees(atan(enu.y - dx * sinL, M + enu.z + dx * cosL));

    // llFrac = sub-degree part (Kahan order: small + small first for precision)
    vec2 llFrac = vec2(
        uOriginLLFracHi.x + (uOriginLLFracLo.x + dLon),
        uOriginLLFracHi.y + (uOriginLLFracLo.y + dLat));
    vec2 ll = uOriginLLInt + llFrac;

    // ── graticule ────────────────────────────────────────────────
    float dist = tHit;

    // Fine grids — use llFrac (float32 precise) for sub-degree spacings
    float fine = 0.0;
    if (dist < uGridFades.w)
        fine = gridLine(ll, 0.1) * 0.45 * distFade(dist, uGridFades.w);
    if (dist < uGridFades.z)
        fine = max(fine, gridLine(llFrac, 0.01) * 0.35 * distFade(dist, uGridFades.z));
    if (dist < uGridFades.y)
        fine = max(fine, gridLine(llFrac, 0.001) * 0.25 * distFade(dist, uGridFades.y));
    if (dist < uGridFades.x)
        fine = max(fine, gridLine(llFrac, 0.0001) * 0.18 * distFade(dist, uGridFades.x));

    // Coarse grids — ECEF Bowring fallback beyond 120°
    float angDist = max(abs(dLon), abs(dLat));
    vec2 llC = ll;
    if (angDist > 110.0) {
        float r  = length(ecef.xy);
        float th = atan(ecef.z * uRadii.x, r * uRadii.z);
        float sT = sin(th), cT = cos(th);
        float ab = uRadii.x * uRadii.x - uRadii.z * uRadii.z;
        llC = vec2(degrees(atan(ecef.y, ecef.x)),
                   degrees(atan(ecef.z + ab / uRadii.z * sT*sT*sT,
                                r - ab / uRadii.x * cT*cT*cT)));
    }
    float coarse = max(
        max(gridLine(llC, 1.0)  * 0.4,
            gridLine(llC, 5.0)  * 0.55),
        max(gridLine(llC, 10.0) * 0.65,
            max(gridLine(llC, 30.0) * 0.8,
                gridLine(llC, 90.0) * 1.0)));

    float line = clamp(max(fine, coarse), 0.0, 1.0);

    // ── shading ──────────────────────────────────────────────────
    vec3 baseColor = mix(uSurfaceColor.rgb, uGratColor.rgb, line);

    // Diffuse lighting
    float diffuse = max(dot(normal, uLightDir), 0.0);
    vec3 lit = baseColor * (uAmbient + (1.0 - uAmbient) * diffuse);

    // Atmosphere rim (Fresnel-like: bright at limb where view ⊥ normal)
    float rim = pow(1.0 - max(dot(normal, -ray), 0.0), uAtmoParams.x);
    lit += uAtmoColor * rim * uAtmoParams.y;

    // ── output ───────────────────────────────────────────────────
    // Smooth limb edge: NdotV → 0 at the horizon → alpha fades → MSAA coverage ramps down.
    // fwidth scales the fade band to pixel size — works at any zoom level.
    float NdotV = max(dot(normal, -ray), 0.0);
    float edgeFade = smoothstep(0.0, max(0.002, fwidth(NdotV) * 2.0), NdotV);
    FragColor = vec4(lit, uSurfaceColor.a * edgeFade);

    vec4 cp = uViewProj * vec4(hitWorld, 1.0);
    // Depth bias: globe always loses to lines/meshes at similar depth.
    // 1e-4 ≈ 0.2m at 1km distance — objects on/near the surface always win.
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) / log2(uFarPlane + 1.0) + 1e-4, 0.0, 1.0);
}
