// Point pick fragment shader — clips to circle, writes pick ID.
#version 450 core

flat in uint vPickId;

layout(location = 0) out uint FragId;

void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    if (dot(c, c) > 1.0) discard;
    FragId = vPickId;
}
