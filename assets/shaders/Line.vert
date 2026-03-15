// Line vertex shader — expands line segments into screen-space quads.
// Each line is 6 vertices (2 triangles). aExpand encodes quad corner:
//   x = side (-1 or +1), y = which endpoint (0 = start, 1 = end)
// Lines thin with distance via perspective scaling.
#version 450 core

layout(location = 0) in vec3 aPos;       // line endpoint A
layout(location = 1) in vec3 aOther;     // line endpoint B
layout(location = 2) in vec2 aExpand;    // quad corner encoding
layout(location = 3) in vec4 aColor;

uniform mat4  uView, uProj;
uniform vec2  uViewportSize;
uniform float uLineWidth;

out vec4  vColor;
out float vEdge;       // [-1,+1] across line width for antialiasing
out float vHalfWidth;  // true half-width in pixels (before AA padding)

void main() {
    // Project both endpoints to clip space
    vec4 cA = uProj * uView * vec4(aPos, 1);
    vec4 cB = uProj * uView * vec4(aOther, 1);

    // Compute screen-space direction and perpendicular
    vec2 sA   = cA.xy / cA.w * uViewportSize * 0.5;
    vec2 sB   = cB.xy / cB.w * uViewportSize * 0.5;
    vec2 dir  = sB - sA;
    float len = length(dir);
    dir       = len > 0.001 ? dir / len : vec2(1, 0);
    vec2 perp = vec2(-dir.y, dir.x);

    // Perspective scaling: lines thin with distance
    float depth     = mix(cA.w, cB.w, aExpand.y);
    float projScale = uProj[1][1] * uViewportSize.y * 0.005;
    float scale     = clamp(projScale / depth, 0.15, 3.0);

    // Expand vertex perpendicular to line direction (+0.5px for AA)
    float trueHW = uLineWidth * 0.5 * scale;
    float hw     = trueHW + 0.5;
    vec2  off    = perp * aExpand.x * hw / (uViewportSize * 0.5);

    // Interpolate clip position and apply offset
    vec4 clip = mix(cA, cB, aExpand.y);
    clip.xy  += off * clip.w;

    gl_Position = clip;
    vColor      = aColor;
    vEdge       = aExpand.x;
    vHalfWidth  = trueHW;
}
