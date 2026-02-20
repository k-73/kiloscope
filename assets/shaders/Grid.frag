#version 450 core
in vec3 vNear, vFar;

uniform mat4 uView, uProj;
uniform vec3 uCamPos;
uniform float uCamDist;

out vec4 FragColor;

// Gaussian-profile grid line: thin bright core + wide soft glow
float SoftLine(vec2 coord, float scale) {
    vec2 c = coord / scale;
    vec2 d = fwidth(c);
    vec2 a = abs(fract(c - 0.5) - 0.5) / d;

    // Sharp core (half-width ~1px)
    float core = max(exp2(-a.x * a.x * 2.0), exp2(-a.y * a.y * 2.0));
    // Broad glow (half-width ~3px)
    float glow = max(exp2(-a.x * a.x * 0.25), exp2(-a.y * a.y * 0.25));

    return core * 0.7 + glow * 0.3;
}

// Soft axis line with gaussian profile
float SoftAxis(float dist, float thickness) {
    float d = abs(dist) / thickness;
    return exp2(-d * d * 2.0);
}

void main() {
    float t = -vNear.z / (vFar.z - vNear.z);
    if (t < 0.0) discard;

    vec3 fp = vNear + t * (vFar - vNear);

    // Layered grid — all decade scales always present, coarser = brighter overlay
    // SoftLine naturally returns ~0 when grid becomes sub-pixel dense
    float g1   = SoftLine(fp.xy, 1.0);
    float g10  = SoftLine(fp.xy, 10.0);
    float g100 = SoftLine(fp.xy, 100.0);

    // Additive color: finer grids persist underneath, coarser ones glow brighter
    vec3 col = vec3(0.20, 0.22, 0.26) * g1
             + vec3(0.28, 0.30, 0.36) * g10
             + vec3(0.42, 0.45, 0.52) * g100;

    // Axis highlights — continuous thickness proportional to camera distance
    float axThick = uCamDist * 0.006;
    float axX = SoftAxis(fp.x, max(axThick, fwidth(fp.x) * 2.0));
    float axY = SoftAxis(fp.y, max(axThick, fwidth(fp.y) * 2.0));
    col += vec3(0.2, 0.8, 0.2) * axX + vec3(0.8, 0.2, 0.2) * axY;

    // Distance fade
    float d = length(fp.xy - uCamPos.xy);
    float fade = 1.0 - smoothstep(uCamDist * 2.5, uCamDist * 10.0, d);

    float alpha = max(max(g1 * 0.25, g10 * 0.40), g100 * 0.60) * fade;
    alpha = max(alpha, max(axX, axY) * fade * 0.75);
    if (alpha < 0.003) discard;

    FragColor = vec4(col, alpha);
    vec4 cp = uProj * uView * vec4(fp, 1);
    gl_FragDepth = cp.z / cp.w * 0.5 + 0.5;
}
