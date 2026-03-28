// WGS84 ellipsoid surface + metric grid + lat/lon graticule.
// vNear is camera-relative (View matrix has eye at origin).
// uCamPos is world position of camera (for converting to world coords).
#version 450 core

in vec3 vNear;     // camera-relative near-plane point
in vec3 vDir;      // unnormalized ray direction

uniform mat4  uViewProj;
uniform vec3  uCamPos;       // camera world position (ENU)
uniform vec3  uEllCenter;    // ellipsoid center, camera-relative
uniform vec3  uRadii;
uniform mat3  uEcefToLocal;
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

    // vNear is camera-relative. ro for ellipsoid = vNear (already cam-relative).
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

    float tEll = (-B - sqrt(disc)) / A;
    if (tEll < 0.0) discard;

    // ── Metric grid (flat plane Z=0 in world coords) ─────────────
    // Camera-relative Z: flat plane is at z = -uCamPos.z (since cam is at uCamPos.z above plane)
    float tPlane = -(vNear.z + uCamPos.z) / rd.z;  // plane at world Z=0 → cam-rel Z = -camPos.z
    float tGrid = clamp(tPlane, 0.0, tEll);
    // fp in world coords = camera-relative hit + camera world position
    vec3 fpWorld = vNear + tGrid * rd + uCamPos;

    float m1 = SoftLine(fpWorld.xy, 10.0)    * 0.25;
    float m2 = SoftLine(fpWorld.xy, 100.0)   * 0.4;
    float m3 = SoftLine(fpWorld.xy, 1000.0)  * 0.6;
    float m4 = SoftLine(fpWorld.xy, 10000.0) * 0.8;
    float metricLine = max(max(m1, m2), max(m3, m4));

    // ── Lat/lon graticule ────────────────────────────────────────
    vec3 hitLocal = ro + rd * tEll;
    vec3 ecef = toEcef * (hitLocal - uEllCenter);
    float lon = degrees(atan(ecef.y, ecef.x));
    float lat = degrees(atan(ecef.z, length(ecef.xy)));

    float g1 = SoftLine(vec2(lon, lat), 1.0)  * 0.4;
    float g2 = SoftLine(vec2(lon, lat), 10.0) * 0.6;
    float g3 = SoftLine(vec2(lon, lat), 90.0) * 0.9;
    float geoLine = max(max(g1, g2), g3);

    // ── Compose ──────────────────────────────────────────────────
    float line = clamp(max(metricLine, geoLine), 0.0, 1.0);
    vec3 col = mix(uSurfaceColor.rgb, uGratColor.rgb, line);

    FragColor = vec4(col, uSurfaceColor.a);

    // Depth: hitWorld in camera-relative, project with camera-relative ViewProj
    vec3 hitCamRel = vNear + tEll * rd;
    vec4 cp = uViewProj * vec4(hitCamRel, 1.0);
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = clamp(log2(max(1e-6, cp.w + 1.0)) * Fcoef_half, 0.0, 1.0);
}
