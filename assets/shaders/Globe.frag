// WGS84 ellipsoid — ray-cast surface with multi-scale lat/lon graticule.
//
// Architecture:
//   - Ellipsoid intersection determines surface visibility and depth
//   - Lat/lon grid uses flat-plane hit (ENU) for stability (no ECEF rotation noise)
//   - Origin hi/lo split gives full double-precision lat/lon via two floats
//   - Fine grids (< 1°) use fractional degrees only → no float32 quantization
#version 450 core

in vec3 vNear;  // camera-relative near-plane point
in vec3 vDir;   // unnormalized ray direction (avoids f.w=0 at large far plane)

uniform mat4  uViewProj;
uniform vec3  uCamPos;         // camera position in world ENU
uniform vec3  uEllCenter;     // Earth center, camera-relative
uniform vec3  uRadii;         // (a, a, b) WGS84 semi-axes
uniform mat3  uEcefToLocal;   // ECEF → ENU rotation (for intersection only)
uniform vec2  uOriginInt;     // floor(origin lat/lon) — integer degrees
uniform vec2  uOriginFracHi;  // fmod(origin, 1°) — high float
uniform vec2  uOriginFracLo;  // fmod(origin, 1°) — low float (double residual)
uniform float uR;             // Earth radius
uniform vec4  uGratColor;
uniform vec4  uSurfaceColor;
uniform float uFarPlane;

out vec4 FragColor;

// Antialiased grid line (identical to Grid.frag)
float SoftLine(vec2 coord, float scale) {
    vec2 c = coord / scale;
    vec2 d = fwidth(c);
    vec2 a = abs(fract(c - 0.5) - 0.5) / d;
    return max(1.0 - smoothstep(0.3, 1.5, a.x),
               1.0 - smoothstep(0.3, 1.5, a.y));
}

void main() {
    vec3 rd = normalize(vDir);

    // ── Ellipsoid intersection (visibility + depth) ──────────────
    mat3 toEcef = transpose(uEcefToLocal);
    vec3 oc_n = (toEcef * (vNear - uEllCenter)) / uRadii;
    vec3 rd_n = (toEcef * rd) / uRadii;

    float A = dot(rd_n, rd_n);
    float B = dot(oc_n, rd_n);
    float C = dot(oc_n, oc_n) - 1.0;
    float disc = B * B - A * C;
    if (disc < 0.0) discard;

    // Stable near-root: C/(-B+√D) avoids cancellation when camera near surface
    float sqD = sqrt(disc);
    float tEll = C / (-B + sqD);
    if (tEll < 0.0) {
        tEll = (-B + sqD) / A;
        if (tEll < 0.0) discard;
    }

    // ── Lat/lon from flat plane (decoupled from ECEF — fully stable) ──
    float tPlane = -(vNear.z + uCamPos.z) / rd.z;
    vec3 hitENU = vNear + tPlane * rd + uCamPos;

    float cosLat = cos(radians(uOriginInt.x + uOriginFracHi.x));
    float dLon = degrees(hitENU.x / (uR * cosLat));
    float dLat = degrees(hitENU.y / uR);

    // Fractional degrees (hi + delta + lo) — precise, 0-1° range
    vec2 llFrac = vec2((uOriginFracHi.y + dLon) + uOriginFracLo.y,
                       (uOriginFracHi.x + dLat) + uOriginFracLo.x);
    vec2 ll = uOriginInt + llFrac;

    // ── Graticule layers ─────────────────────────────────────────
    // Fine (< 1°): use llFrac → full float32 precision in 0-1 range
    // Coarse (≥ 1°): use ll → adequate precision at those scales
    float surfDist = tEll;

    float line = max(
        max(max(SoftLine(llFrac, 0.001) * 0.15 * smoothstep(5000.0,    500.0,    surfDist),
                SoftLine(llFrac, 0.01)  * 0.25 * smoothstep(50000.0,   5000.0,   surfDist)),
            max(SoftLine(llFrac, 0.1)   * 0.35 * smoothstep(500000.0,  50000.0,  surfDist),
                SoftLine(ll, 1.0)       * 0.45 * smoothstep(2000000.0, 500000.0, surfDist))),
        max(max(SoftLine(ll, 5.0)  * 0.55 * smoothstep(5000000.0, 2000000.0, surfDist),
                SoftLine(ll, 10.0) * 0.65),
            max(SoftLine(ll, 30.0) * 0.8,
                SoftLine(ll, 90.0) * 1.0)));

    FragColor = vec4(mix(uSurfaceColor.rgb, uGratColor.rgb, clamp(line, 0.0, 1.0)),
                     uSurfaceColor.a);

    // ── Depth from ellipsoid (correct curved surface) ────────────
    vec4 cp = uViewProj * vec4(vNear + tEll * rd + uCamPos, 1.0);
    float Fcoef = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) * Fcoef, 0.0, 1.0);
}
