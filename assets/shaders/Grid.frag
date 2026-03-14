#version 450 core
in vec3 vNear, vFar;

uniform mat4 uViewProj;
uniform vec3 uCamPos;
uniform float uCamDist;

uniform float uScaleFine, uScaleMedium, uScaleCoarse;
uniform vec4  uColorFine, uColorMedium, uColorCoarse;  // rgb + alpha
uniform vec4  uAxisXColor, uAxisYColor;                 // rgb + alpha
uniform float uAxisThickness;
uniform int   uAxisScaleWithCam;
uniform float uFadeStart, uFadeEnd;

out vec4 FragColor;

float SoftLine(vec2 coord, float scale) {
    vec2 c = coord / scale;
    vec2 d = fwidth(c);
    vec2 a = abs(fract(c - 0.5) - 0.5) / d;
    float lx = 1.0 - smoothstep(0.3, 1.5, a.x);
    float ly = 1.0 - smoothstep(0.3, 1.5, a.y);
    return max(lx, ly);
}

float SoftAxis(float dist, float thickness) {
    return 1.0 - smoothstep(thickness * 0.35, thickness, abs(dist));
}

void main() {
    float t = -vNear.z / (vFar.z - vNear.z);
    if (t < 0.0) discard;

    vec3 fp = vNear + t * (vFar - vNear);

    // Layered grid at three scales
    float g1   = SoftLine(fp.xy, uScaleFine);
    float g10  = SoftLine(fp.xy, uScaleMedium);
    float g100 = SoftLine(fp.xy, uScaleCoarse);

    // Layer compositing: fine → medium over fine → coarse over result
    vec3  gridCol = uColorFine.rgb;
    float gridAlpha = g1 * uColorFine.a;

    float medA = g10 * uColorMedium.a;
    float prevA = gridAlpha;
    gridAlpha = medA + prevA * (1.0 - medA);
    if (gridAlpha > 0.001)
        gridCol = (uColorMedium.rgb * medA + gridCol * prevA * (1.0 - medA)) / gridAlpha;

    float coaA = g100 * uColorCoarse.a;
    prevA = gridAlpha;
    gridAlpha = coaA + prevA * (1.0 - coaA);
    if (gridAlpha > 0.001)
        gridCol = (uColorCoarse.rgb * coaA + gridCol * prevA * (1.0 - coaA)) / gridAlpha;

    // Axis layer (composited "over" grid)
    float axThick = (uAxisScaleWithCam != 0) ? uCamDist * uAxisThickness : uAxisThickness;
    float axX = SoftAxis(fp.x, max(axThick, fwidth(fp.x) * 2.0));
    float axY = SoftAxis(fp.y, max(axThick, fwidth(fp.y) * 2.0));

    // Per-axis alpha from vec4.a
    float axAlphaX = axY * uAxisXColor.a;  // axY = distance from Y=0 = X axis
    float axAlphaY = axX * uAxisYColor.a;  // axX = distance from X=0 = Y axis
    float axA = clamp(max(axAlphaX, axAlphaY), 0.0, 1.0);

    vec3 axCol = uAxisYColor.rgb * axX + uAxisXColor.rgb * axY;
    float axSum = axX + axY;
    if (axSum > 0.001) axCol /= axSum;

    // Porter-Duff "over": axis on top of grid
    float alpha = axA + gridAlpha * (1.0 - axA);
    vec3  col   = alpha > 0.001
        ? (axCol * axA + gridCol * gridAlpha * (1.0 - axA)) / alpha
        : vec3(0);

    // Distance fade
    float d = length(fp.xy - uCamPos.xy);
    float fade = 1.0 - smoothstep(uCamDist * uFadeStart, uCamDist * uFadeEnd, d);
    alpha *= fade;
    if (alpha < 0.003) discard;

    FragColor = vec4(col, alpha);
    vec4 cp = uViewProj * vec4(fp, 1);
    gl_FragDepth = cp.z / cp.w * 0.5 + 0.5;
}
