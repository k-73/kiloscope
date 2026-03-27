// WGS84 ellipsoid — ray-ellipsoid intersection with multi-scale lat/lon graticule.
#version 450 core

in vec3 vNear, vFar;

uniform mat4  uViewProj;
uniform vec3  uCamPos;
uniform vec3  uEllCenter;
uniform vec3  uRadii;
uniform mat3  uEcefToLocal;
uniform float uGratSpacing;
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

void CompOver(inout vec3 bg, inout float bgA, vec3 fg, float fgA) {
    float outA = fgA + bgA * (1.0 - fgA);
    if (outA > 0.001) bg = (fg * fgA + bg * bgA * (1.0 - fgA)) / outA;
    bgA = outA;
}

void main() {
    vec3 ro = vNear - uCamPos;
    vec3 rd = normalize(vFar - vNear);

    // Ray-ellipsoid intersection
    vec3 oc   = (ro - uEllCenter) / uRadii;
    vec3 rd_n = rd / uRadii;

    float A    = dot(rd_n, rd_n);
    float B    = dot(oc, rd_n);
    float C    = dot(oc, oc) - 1.0;
    float disc = B * B - A * C;
    if (disc < 0.0) discard;

    float t = (-B - sqrt(disc)) / A;
    if (t < 0.0) discard;

    vec3 hitLocal = ro + rd * t;
    vec3 hitWorld = hitLocal + uCamPos;

    // Lat/lon from ECEF
    vec3 ecef = transpose(uEcefToLocal) * (hitLocal - uEllCenter);
    float lon = degrees(atan(ecef.y, ecef.x));
    float lat = degrees(atan(ecef.z, length(ecef.xy)));
    vec2 ll = vec2(lon, lat);

    // Multi-scale graticule (3 layers: spacing, 10x, 100x)
    float g1 = SoftLine(ll, uGratSpacing);
    float g2 = SoftLine(ll, uGratSpacing * 10.0);
    float g3 = SoftLine(ll, uGratSpacing * 100.0);

    // Compose surface + graticule layers (only alpha varies, color stays)
    vec3  col   = uSurfaceColor.rgb;
    float alpha = uSurfaceColor.a;
    CompOver(col, alpha, uGratColor.rgb, g1 * uGratColor.a * 0.5);
    CompOver(col, alpha, uGratColor.rgb, g2 * uGratColor.a * 0.8);
    CompOver(col, alpha, uGratColor.rgb, g3 * uGratColor.a);

    if (alpha < 0.003) discard;

    FragColor    = vec4(col, alpha);
    vec4 cp      = uViewProj * vec4(hitWorld, 1.0);
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = log2(max(1e-6, cp.w + 1.0)) * Fcoef_half;
}
