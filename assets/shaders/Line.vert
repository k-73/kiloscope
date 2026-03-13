#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aOther;
layout(location = 2) in vec2 aExpand;   // x: side (-1/+1), y: which end (0/1)
layout(location = 3) in vec4 aColor;

uniform mat4 uView, uProj;
uniform vec2 uViewportSize;
uniform float uLineWidth;

out vec4 vColor;
out float vEdge;
out float vHalfWidth;

void main() {
    vec4 cA = uProj * uView * vec4(aPos, 1);
    vec4 cB = uProj * uView * vec4(aOther, 1);

    vec2 sA = cA.xy / cA.w * uViewportSize * 0.5;
    vec2 sB = cB.xy / cB.w * uViewportSize * 0.5;

    vec2 dir = sB - sA;
    float len = length(dir);
    dir = len > 0.001 ? dir / len : vec2(1, 0);
    vec2 perp = vec2(-dir.y, dir.x);

    // perspective scaling: lines thin with distance
    float depth = mix(cA.w, cB.w, aExpand.y);
    float projScale = uProj[1][1] * uViewportSize.y * 0.005;
    float scale = clamp(projScale / depth, 0.15, 3.0);

    float trueHW = uLineWidth * 0.5 * scale;
    float hw = trueHW + 1.0;
    vec2 off = perp * aExpand.x * hw / (uViewportSize * 0.5);

    vec4 clip = mix(cA, cB, aExpand.y);
    clip.xy += off * clip.w;

    gl_Position = clip;
    vColor = aColor;
    vEdge = aExpand.x;
    vHalfWidth = trueHW;
}
