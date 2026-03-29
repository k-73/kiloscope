// Point fragment shader — circular disc + logarithmic depth.
#version 450 core

in vec4  vColor;
in float vLogZ;

uniform float uFarPlane;

out vec4 FragColor;

void main() {
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = log2(vLogZ) * Fcoef_half;

    vec2  c = gl_PointCoord * 2.0 - 1.0;
    float d = dot(c, c);
    if (d > 1.0) discard;

    float edge = max(fwidth(d) * 2.0, 0.15);
    float coverage = (1.0 - smoothstep(1.0 - edge, 1.0, d)) * vColor.a;
    if (coverage < 0.02) discard;
    FragColor = vec4(vColor.rgb, coverage);
}
