#version 450 core
in vec4 vColor;
in float vEdge;
in float vHalfWidth;

out vec4 FragColor;

void main() {
    float edge = vHalfWidth / (vHalfWidth + 0.5);
    float coverage = 1.0 - smoothstep(edge, 1.0, abs(vEdge));
    coverage *= vColor.a;
    if (coverage < 0.02) discard;
    FragColor = vec4(vColor.rgb, coverage);
}
