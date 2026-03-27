// WGS84 ellipsoid — ray-ellipsoid intersection with lat/lon graticule.
#version 450 core

in vec3 vNear, vFar;

uniform mat4  uViewProj;
uniform vec3  uCamPos;
uniform vec3  uEllCenter;     // ellipsoid center relative to camera (scene coords)
uniform vec3  uRadii;         // (a, a, b) semi-axes
uniform mat3  uEcefToLocal;   // ECEF → scene rotation
uniform float uGratSpacing;   // graticule spacing (degrees)
uniform vec4  uGratColor;
uniform vec4  uSurfaceColor;

out vec4 FragColor;

float SoftLine(vec2 coord, float scale) {
    vec2 c = coord / scale;
    vec2 d = fwidth(c);
    vec2 a = abs(fract(c - 0.5) - 0.5) / d;
    return max(1.0 - smoothstep(0.3, 1.5, a.x),
               1.0 - smoothstep(0.3, 1.5, a.y));
}

void main() {
    vec3 ro = vNear - uCamPos;
    vec3 rd = normalize(vFar - vNear);

    // Ray-ellipsoid: normalize to unit sphere space
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

    // Lat/lon from ECEF for graticule
    vec3 ecef = transpose(uEcefToLocal) * (hitLocal - uEllCenter);
    float lon = degrees(atan(ecef.y, ecef.x));
    float lat = degrees(atan(ecef.z, length(ecef.xy)));

    float grat = SoftLine(vec2(lon, lat), uGratSpacing);

    vec3  col   = mix(uSurfaceColor.rgb, uGratColor.rgb, grat * uGratColor.a);
    float alpha = uSurfaceColor.a + grat * uGratColor.a * (1.0 - uSurfaceColor.a);
    if (alpha < 0.003) discard;

    FragColor    = vec4(col, alpha);
    vec4 cp      = uViewProj * vec4(hitWorld, 1.0);
    gl_FragDepth = clamp(cp.z / cp.w * 0.5 + 0.5, 0.0, 1.0);
}
