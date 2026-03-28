// Pick line vertex shader — quad expansion + logarithmic depth.
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aOther;
layout(location = 2) in vec2 aExpand;
layout(location = 3) in vec4 aColor;
layout(location = 4) in uint aPickId;
layout(location = 5) in float aWidth;

uniform mat4  uViewProj;
uniform float uProjScale;
uniform vec2  uViewportSize;
uniform float uFarPlane;

flat out uint vPickId;
out float vLogZ;

void main() {
    vec4 cA = uViewProj * vec4(aPos, 1);
    vec4 cB = uViewProj * vec4(aOther, 1);

    vec2 sA   = cA.xy / cA.w * uViewportSize * 0.5;
    vec2 sB   = cB.xy / cB.w * uViewportSize * 0.5;
    vec2 dir  = sB - sA;
    float len = length(dir);
    dir       = len > 0.001 ? dir / len : vec2(1, 0);
    vec2 perp = vec2(-dir.y, dir.x);

    float depth     = mix(cA.w, cB.w, aExpand.y);
    float projScale = uProjScale;
    float scale     = clamp(projScale / depth, 0.15, 3.0);

    float hw  = aWidth * 0.5 * scale + 0.5;
    vec2  off = perp * aExpand.x * hw / (uViewportSize * 0.5);

    vec4 clip = mix(cA, cB, aExpand.y);
    clip.xy  += off * clip.w;

    gl_Position = clip;
    float Fcoef = 2.0 / log2(uFarPlane + 1.0);
    gl_Position.z = (log2(max(1e-6, gl_Position.w + 1.0)) * Fcoef - 1.0) * gl_Position.w;
    vLogZ = 1.0 + gl_Position.w;

    vPickId = aPickId;
}
