// Globe vertex shader — fullscreen quad for ray-ellipsoid intersection.
// Same pattern as Grid.vert.
#version 450 core

uniform mat4 uInvViewProj;

out vec3 vNear;
out vec3 vDir;   // unnormalized ray direction (avoids inf when far >> near)

void main() {
    vec2 pos[6] = vec2[](
        vec2(-1, -1), vec2(1, -1), vec2(1, 1),
        vec2(-1, -1), vec2(1,  1), vec2(-1, 1)
    );
    vec2 p = pos[gl_VertexID];

    vec4 n = uInvViewProj * vec4(p, -1, 1);
    vec4 f = uInvViewProj * vec4(p,  1, 1);
    vNear = n.xyz / n.w;
    // Ray direction without dividing by f.w (which can be zero when far >> near):
    //   exact: vFar - vNear = f.xyz/f.w - n.xyz/n.w = (f.xyz*n.w - n.xyz*f.w) / (f.w*n.w)
    //   we drop the scalar denominator (n.w*f.w) — only direction matters.
    vDir = f.xyz * n.w - n.xyz * f.w;

    gl_Position = vec4(p, 0, 1);
}
