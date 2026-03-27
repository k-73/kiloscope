// Line vertex shader — expands line segments into screen-space quads.
// Logarithmic depth for large-scale scenes.
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aOther;
layout(location = 2) in vec2 aExpand;
layout(location = 3) in vec4 aColor;

uniform mat4  uView, uProj;
uniform vec2  uViewportSize;
uniform float uLineWidth;
uniform float uFarPlane;

out vec4  vColor;
out float vEdge;
out float vHalfWidth;
out float vLogZ;

void main() {
    vec4 cA = uProj * uView * vec4(aPos, 1);
    vec4 cB = uProj * uView * vec4(aOther, 1);

    vec2 sA   = cA.xy / cA.w * uViewportSize * 0.5;
    vec2 sB   = cB.xy / cB.w * uViewportSize * 0.5;
    vec2 dir  = sB - sA;
    float len = length(dir);
    dir       = len > 0.001 ? dir / len : vec2(1, 0);
    vec2 perp = vec2(-dir.y, dir.x);

    float depth     = mix(cA.w, cB.w, aExpand.y);
    float projScale = uProj[1][1] * uViewportSize.y * 0.005;
    float scale     = clamp(projScale / depth, 0.15, 3.0);

    float trueHW = uLineWidth * 0.5 * scale;
    float hw     = trueHW + 0.5;
    vec2  off    = perp * aExpand.x * hw / (uViewportSize * 0.5);

    vec4 clip = mix(cA, cB, aExpand.y);
    clip.xy  += off * clip.w;

    gl_Position = clip;

    // Logarithmic depth
    float Fcoef = 2.0 / log2(uFarPlane + 1.0);
    gl_Position.z = (log2(max(1e-6, gl_Position.w + 1.0)) * Fcoef - 1.0) * gl_Position.w;
    vLogZ = 1.0 + gl_Position.w;

    vColor     = aColor;
    vEdge      = aExpand.x;
    vHalfWidth = trueHW;
}
