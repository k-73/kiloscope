// Infinite ground grid — renders on the XY plane via ray-plane intersection.
// Three grid layers (fine/medium/coarse) composited with Porter-Duff "over".
// Axis lines drawn on top. Fades with camera distance.
#version 450 core

in vec3 vNear;
in vec3 vDir;    // unnormalized ray direction from vertex shader

uniform mat4  uViewProj;
uniform vec3  uCamPos;
uniform float uCamDist;

// Grid scales and colors (rgb + alpha = line opacity)
uniform float uScaleFine, uScaleMedium, uScaleCoarse;
uniform vec4  uColorFine, uColorMedium, uColorCoarse;

// Axis styling
uniform vec4  uAxisXColor, uAxisYColor;
uniform float uAxisThickness;
uniform int   uAxisScaleWithCam;

// Distance fade range (multiplied by camera distance)
uniform float uFadeStart, uFadeEnd;
uniform float uFarPlane;

out vec4 FragColor;

// ── helpers ──────────────────────────────────────────────────────────

// Antialiased grid line using screen-space derivatives
float SoftLine(vec2 coord, float scale) {
    vec2 c  = coord / scale;
    vec2 d  = fwidth(c);
    vec2 a  = abs(fract(c - 0.5) - 0.5) / d;
    float lx = 1.0 - smoothstep(0.3, 1.5, a.x);
    float ly = 1.0 - smoothstep(0.3, 1.5, a.y);
    return max(lx, ly);
}

// Antialiased axis line based on distance from axis
float SoftAxis(float dist, float thickness) {
    return 1.0 - smoothstep(thickness * 0.35, thickness, abs(dist));
}

// Porter-Duff "over": composites foreground (fg) over background (bg)
void CompOver(inout vec3 bgCol, inout float bgA, vec3 fgCol, float fgA) {
    float outA = fgA + bgA * (1.0 - fgA);
    if (outA > 0.001)
        bgCol = (fgCol * fgA + bgCol * bgA * (1.0 - fgA)) / outA;
    bgA = outA;
}

// ── main ─────────────────────────────────────────────────────────────

void main() {
    // Ray-plane intersection (world Z=0, camera-relative coords)
    vec3  rd = normalize(vDir);
    if (abs(rd.z) < 1e-6) discard;
    float t  = -(vNear.z + uCamPos.z) / rd.z;
    if (t < 0.0) discard;
    vec3 fp = vNear + t * rd + uCamPos;  // convert to world coords for grid

    // Three grid layers
    float g1   = SoftLine(fp.xy, uScaleFine);
    float g10  = SoftLine(fp.xy, uScaleMedium);
    float g100 = SoftLine(fp.xy, uScaleCoarse);

    // Composite: fine → medium over fine → coarse over result
    vec3  gridCol = uColorFine.rgb;
    float gridA   = g1 * uColorFine.a;
    CompOver(gridCol, gridA, uColorMedium.rgb, g10 * uColorMedium.a);
    CompOver(gridCol, gridA, uColorCoarse.rgb, g100 * uColorCoarse.a);

    // Axis lines (composited over grid)
    float axThick = (uAxisScaleWithCam != 0) ? uCamDist * uAxisThickness : uAxisThickness;
    float axX = SoftAxis(fp.x, max(axThick, fwidth(fp.x) * 2.0));
    float axY = SoftAxis(fp.y, max(axThick, fwidth(fp.y) * 2.0));

    float axAlphaX = axY * uAxisXColor.a;  // X axis = line along Y=0
    float axAlphaY = axX * uAxisYColor.a;  // Y axis = line along X=0
    float axA = clamp(max(axAlphaX, axAlphaY), 0.0, 1.0);

    vec3  axCol = uAxisYColor.rgb * axX + uAxisXColor.rgb * axY;
    float axSum = axX + axY;
    if (axSum > 0.001) axCol /= axSum;

    // Final composite: axis over grid
    float alpha = axA + gridA * (1.0 - axA);
    vec3  col   = alpha > 0.001
        ? (axCol * axA + gridCol * gridA * (1.0 - axA)) / alpha
        : vec3(0);

    // Distance fade
    float d      = length(fp.xy - uCamPos.xy);
    float scale  = 1.0 + uCamDist;
    float fade   = 1.0 - smoothstep(uFadeStart * scale, uFadeEnd * scale, d);
    alpha *= fade;
    if (alpha < 0.003) discard;

    FragColor    = vec4(col, alpha);
    vec4 cp      = uViewProj * vec4(fp, 1);
    float Fcoef_half = 1.0 / log2(uFarPlane + 1.0);
    gl_FragDepth = log2(max(1e-6, cp.w + 1.0)) * Fcoef_half + 1e-4;
}
