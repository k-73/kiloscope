// Line fragment shader — smooth antialiased edge via coverage falloff.
#version 450 core

in vec4  vColor;
in float vEdge;       // [-1,+1] across line width
in float vHalfWidth;  // true half-width in pixels

out vec4 FragColor;

void main() {
    float edge     = vHalfWidth / (vHalfWidth + 0.5);
    float coverage = 1.0 - smoothstep(edge, 1.0, abs(vEdge));
    coverage *= vColor.a;
    if (coverage < 0.02) discard;
    FragColor = vec4(vColor.rgb, coverage);
}
