// Mesh pick vertex shader — minimal transform for color-ID picking.
// Pick ID is a per-draw uniform (same for all vertices in one draw call).
#version 450 core

layout(location = 0) in vec3 aPos;

uniform mat4 uViewProj;

void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
