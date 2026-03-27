// Globe vertex shader — fullscreen quad for ray-ellipsoid intersection.
// Same pattern as Grid.vert.
#version 450 core

uniform mat4 uInvViewProj;

out vec3 vNear, vFar;

void main() {
    vec2 pos[6] = vec2[](
        vec2(-1, -1), vec2(1, -1), vec2(1, 1),
        vec2(-1, -1), vec2(1,  1), vec2(-1, 1)
    );
    vec2 p = pos[gl_VertexID];

    vec4 n = uInvViewProj * vec4(p, -1, 1);
    vec4 f = uInvViewProj * vec4(p,  1, 1);
    vNear = n.xyz / n.w;
    vFar  = f.xyz / f.w;

    gl_Position = vec4(p, 0, 1);
}
