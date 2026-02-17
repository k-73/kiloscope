#version 450 core
uniform mat4 uView, uProj;

out vec3 vNear, vFar;

void main() {
    vec2 pos[6] = vec2[](vec2(-1,-1), vec2(1,-1), vec2(1,1),
                         vec2(-1,-1), vec2(1, 1), vec2(-1,1));
    vec2 p = pos[gl_VertexID];
    mat4 inv = inverse(uProj * uView);
    vec4 n = inv * vec4(p, 0, 1); vNear = n.xyz / n.w;
    vec4 f = inv * vec4(p, 1, 1); vFar  = f.xyz / f.w;
    gl_Position = vec4(p, 0, 1);
}
