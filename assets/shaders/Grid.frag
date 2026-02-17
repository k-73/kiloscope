#version 450 core
in vec3 vNear, vFar;

uniform mat4 uView, uProj;
uniform vec3 uCamPos;
uniform float uCamDist;

out vec4 FragColor;

float GridLine(vec2 co, float scale) {
    vec2 g = abs(fract(co / scale - 0.5) - 0.5) / fwidth(co / scale);
    return 1.0 - min(min(g.x, g.y), 1.0);
}

void main() {
    float t = -vNear.y / (vFar.y - vNear.y);
    if (t < 0.0) discard;

    vec3 fp = vNear + t * (vFar - vNear);

    float sc = uCamDist > 200.0 ? 100.0 : uCamDist > 40.0 ? 10.0 : 1.0;
    float g1 = GridLine(fp.xz, sc);
    float g2 = GridLine(fp.xz, sc * 10.0);

    float at = sc * 0.04;
    float axX = float(abs(fp.x) < at);
    float axZ = float(abs(fp.z) < at);

    vec3 col = vec3(0.35) * g1 + vec3(0.55) * g2;
    col += vec3(0.85, 0.15, 0.15) * axX + vec3(0.15, 0.15, 0.85) * axZ;

    float d = length(fp.xz - uCamPos.xz);
    float fade = 1.0 - smoothstep(uCamDist * 2.5, uCamDist * 10.0, d);

    float alpha = max(g1 * 0.4, g2 * 0.6) * fade;
    alpha = max(alpha, max(axX, axZ) * fade * 0.8);
    if (alpha < 0.005) discard;

    FragColor = vec4(col, alpha);
    vec4 cp = uProj * uView * vec4(fp, 1);
    gl_FragDepth = cp.z / cp.w * 0.5 + 0.5;
}
