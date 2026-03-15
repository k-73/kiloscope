// Grid vertex shader — fullscreen quad that computes near/far ray
// for ray-plane intersection in the fragment shader.
#version 450 core

uniform mat4 uView, uProj;

out vec3 vNear, vFar;

void main() {
    // Fullscreen quad (2 triangles, 6 vertices via gl_VertexID)
    vec2 pos[6] = vec2[](
        vec2(-1, -1), vec2(1, -1), vec2(1, 1),
        vec2(-1, -1), vec2(1,  1), vec2(-1, 1)
    );
    vec2 p = pos[gl_VertexID];

    // Unproject to world space at near and far planes
    mat4 inv = inverse(uProj * uView);
    vec4 n = inv * vec4(p, 0, 1);
    vec4 f = inv * vec4(p, 1, 1);
    vNear = n.xyz / n.w;
    vFar  = f.xyz / f.w;

    gl_Position = vec4(p, 0, 1);
}
