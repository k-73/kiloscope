// Point vertex shader — perspective-scaled point rendering.
// Point size thins with distance (clamped to [1, 128] pixels).
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;

uniform mat4  uView, uProj;
uniform float uPointSize;
uniform vec2  uViewportSize;

out vec4 vColor;

void main() {
    vec4 clip = uProj * uView * vec4(aPos, 1.0);
    gl_Position = clip;

    // Scale point size by perspective projection
    float projScale = uProj[1][1] * uViewportSize.y * 0.5;
    gl_PointSize = clamp(uPointSize * projScale / clip.w, 1.0, 128.0);

    vColor = aColor;
}
