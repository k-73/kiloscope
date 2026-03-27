// Point pick vertex shader — perspective scaling + logarithmic depth.
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in uint aPickId;

uniform mat4  uView, uProj;
uniform float uPointSize;
uniform vec2  uViewportSize;
uniform float uFarPlane;

flat out uint vPickId;
out float vLogZ;

void main() {
    vec4 clip = uProj * uView * vec4(aPos, 1.0);
    gl_Position = clip;

    float Fcoef = 2.0 / log2(uFarPlane + 1.0);
    gl_Position.z = (log2(max(1e-6, gl_Position.w + 1.0)) * Fcoef - 1.0) * gl_Position.w;
    vLogZ = 1.0 + clip.w;

    float projScale = uProj[1][1] * uViewportSize.y * 0.5;
    gl_PointSize = clamp(uPointSize * projScale / clip.w, 1.0, 128.0);

    vPickId = aPickId;
}
