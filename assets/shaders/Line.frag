// Line fragment shader — smooth antialiased edge + logarithmic depth.
#version 450 core

in vec4  vColor;
in float vEdge;
in float vHalfWidth;
in float vLogZ;

uniform float uFarPlane;

out vec4 FragColor;

void main() {
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = log2(vLogZ) * Fcoef_half;

    float edge     = vHalfWidth / (vHalfWidth + 0.5);
    float coverage = 1.0 - smoothstep(edge, 1.0, abs(vEdge));
    coverage *= vColor.a;
    if (coverage < 0.02) discard;
    FragColor = vec4(vColor.rgb, coverage);
}
