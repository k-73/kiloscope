// Point pick vertex shader — same perspective scaling as Point.vert,
// outputs pick ID for object selection.
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in uint aPickId;

uniform mat4  uView, uProj;
uniform float uPointSize;
uniform vec2  uViewportSize;

flat out uint vPickId;

void main() {
    vec4 clip = uProj * uView * vec4(aPos, 1.0);
    gl_Position = clip;

    float projScale = uProj[1][1] * uViewportSize.y * 0.5;
    gl_PointSize = clamp(uPointSize * projScale / clip.w, 1.0, 128.0);

    vPickId = aPickId;
}
