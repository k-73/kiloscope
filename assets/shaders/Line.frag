#version 450 core
in vec4 vColor;
in float vEdge;
in float vHalfWidth;

out vec4 FragColor;

void main() {
    float aa = 1.5 / (vHalfWidth + 1.0);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, abs(vEdge));
    FragColor = vec4(vColor.rgb, vColor.a * alpha);
}
