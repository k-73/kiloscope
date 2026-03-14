#version 450 core
in vec4 vColor;

out vec4 FragColor;

void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float d = dot(c, c);
    if (d > 1.0) discard;
    float coverage = (1.0 - smoothstep(0.8, 1.0, d)) * vColor.a;
    if (coverage < 0.02) discard;
    FragColor = vec4(vColor.rgb, coverage);
}
