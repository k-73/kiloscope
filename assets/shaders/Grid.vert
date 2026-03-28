// Grid vertex shader — fullscreen quad that computes near/far ray
// for ray-plane intersection in the fragment shader.
#version 450 core

uniform mat4 uInvViewProj;

out vec3 vNear;
out vec3 vDir;   // unnormalized ray direction (avoids inf when far >> near)

void main() {
    // Fullscreen quad (2 triangles, 6 vertices via gl_VertexID)
    vec2 pos[6] = vec2[](
        vec2(-1, -1), vec2(1, -1), vec2(1, 1),
        vec2(-1, -1), vec2(1,  1), vec2(-1, 1)
    );
    vec2 p = pos[gl_VertexID];

    // Unproject to world space at near and far planes (OpenGL NDC z: -1..+1)
    vec4 n = uInvViewProj * vec4(p, -1, 1);
    vec4 f = uInvViewProj * vec4(p,  1, 1);
    vNear = n.xyz / n.w;
    // Ray direction without dividing by f.w (which can be zero when far >> near):
    //   exact: vFar - vNear = (f.xyz*n.w - n.xyz*f.w) / (f.w*n.w)
    //   we drop the scalar denominator — only direction matters.
    vDir = f.xyz * n.w - n.xyz * f.w;

    gl_Position = vec4(p, 0, 1);
}
