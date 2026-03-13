#version 450 core
in vec3 vNear, vFar;

uniform mat4 uView, uProj;
uniform vec3 uCamPos;
uniform float uCamDist;

uniform float uScaleFine, uScaleMedium, uScaleCoarse;
uniform vec3  uColorFine, uColorMedium, uColorCoarse;
uniform float uAlphaFine, uAlphaMedium, uAlphaCoarse;
uniform vec3  uAxisXColor, uAxisYColor;
uniform float uAxisThickness, uAxisAlpha;
uniform float uFadeStart, uFadeEnd;

out vec4 FragColor;

// Gaussian-profile grid line: thin bright core + wide soft glow
float SoftLine(vec2 coord, float scale) {
    vec2 c = coord / scale;
    vec2 d = fwidth(c);
    vec2 a = abs(fract(c - 0.5) - 0.5) / d;

    float core = max(exp2(-a.x * a.x * 4.0), exp2(-a.y * a.y * 4.0));
    float glow = max(exp2(-a.x * a.x * 0.5), exp2(-a.y * a.y * 0.5));

    return core * 0.8 + glow * 0.2;
}

float SoftAxis(float dist, float thickness) {
    float d = abs(dist) / thickness;
    return exp2(-d * d * 2.0);
}

void main() {
    float t = -vNear.z / (vFar.z - vNear.z);
    if (t < 0.0) discard;

    vec3 fp = vNear + t * (vFar - vNear);

    // Layered grid at three scales
    float g1   = SoftLine(fp.xy, uScaleFine);
    float g10  = SoftLine(fp.xy, uScaleMedium);
    float g100 = SoftLine(fp.xy, uScaleCoarse);

    vec3 col = uColorFine * g1 + uColorMedium * g10 + uColorCoarse * g100;

    // Axis highlights
    float axThick = uCamDist * uAxisThickness;
    float axX = SoftAxis(fp.x, max(axThick, fwidth(fp.x) * 2.0));
    float axY = SoftAxis(fp.y, max(axThick, fwidth(fp.y) * 2.0));
    col += uAxisYColor * axX + uAxisXColor * axY;

    // Distance fade
    float d = length(fp.xy - uCamPos.xy);
    float fade = 1.0 - smoothstep(uCamDist * uFadeStart, uCamDist * uFadeEnd, d);

    float alpha = max(max(g1 * uAlphaFine, g10 * uAlphaMedium), g100 * uAlphaCoarse) * fade;
    alpha = max(alpha, max(axX, axY) * fade * uAxisAlpha);
    if (alpha < 0.003) discard;

    FragColor = vec4(col, alpha);
    vec4 cp = uProj * uView * vec4(fp, 1);
    gl_FragDepth = cp.z / cp.w * 0.5 + 0.5;
}
