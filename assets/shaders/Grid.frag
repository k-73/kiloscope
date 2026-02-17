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

    float sc = uCamDist > 200.0 ? 100.0 : uCamDist > 40.0 ? 10.0 : 1.0;
    float g1 = SoftLine(fp.xy, sc);
    float g2 = SoftLine(fp.xy, sc * 10.0);

    // Soft axis highlights — each axis uses its own screen-space derivative
    float axX = SoftAxis(fp.x, max(sc * 0.06, fwidth(fp.x) * 2.0));
    float axY = SoftAxis(fp.y, max(sc * 0.06, fwidth(fp.y) * 2.0));

    // Subtle blue-tinted grid, brighter major lines
    vec3 col = vec3(0.28, 0.30, 0.35) * g1 + vec3(0.45, 0.48, 0.55) * g2;
    col += vec3(0.2, 0.8, 0.2) * axX + vec3(0.8, 0.2, 0.2) * axY;

    // Distance fade
    float d = length(fp.xy - uCamPos.xy);
    float fade = 1.0 - smoothstep(uCamDist * 2.5, uCamDist * 10.0, d);

    float alpha = max(g1 * 0.35, g2 * 0.55) * fade;
    alpha = max(alpha, max(axX, axY) * fade * 0.75);
    if (alpha < 0.003) discard;

    FragColor = vec4(col, alpha);
    vec4 cp = uProj * uView * vec4(fp, 1);
    gl_FragDepth = cp.z / cp.w * 0.5 + 0.5;
}
