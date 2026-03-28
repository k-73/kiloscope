#include "Render/DrawGlobe.hpp"
#include "Render/DrawState.hpp"

namespace Kilo::Render {

static Shader sGlobeShader;
static GLuint sGlobeVao = 0;

// ── init / shutdown ─────────────────────────────────────────────────

void InitGlobe(const std::string& dir) {
    sGlobeShader = Shader(dir + "/Globe.vert", dir + "/Globe.frag");
    glCreateVertexArrays(1, &sGlobeVao);
}

void ShutdownGlobe() {
    sGlobeShader = {};
    if (sGlobeVao) { glDeleteVertexArrays(1, &sGlobeVao); sGlobeVao = 0; }
}

// ── API ─────────────────────────────────────────────────────────────

void SetOrigin(double lat, double lon, double alt)                   { ctx().geoRef.Set(lat, lon, alt); }
void SetOrigin(const char* name, double lat, double lon, double alt) { GetScene(name).geoRef.Set(lat, lon, alt); }
void Globe()                        { ctx().globeCfg.enabled = true; }
void Globe(const GlobeConfig& cfg)  { ctx().globeCfg = cfg; ctx().globeCfg.enabled = true; }
GlobeConfig& GetGlobe()             { return ctx().globeCfg; }
GlobeConfig& GetGlobe(const char* n){ return GetScene(n).globeCfg; }

glm::dvec3 GeoToLocal(double lat, double lon, double alt) {
    auto& gr = ctx().geoRef;
    if (!gr.valid) return glm::dvec3(0.0);
    return glm::dvec3(glm::transpose(glm::dmat3(ctx().frameMat)) * gr.ToInternal(lat, lon, alt));
}

glm::dvec3 GeoToLocal(const char* name, double lat, double lon, double alt) {
    auto& sc = GetScene(name);
    if (!sc.geoRef.valid) return glm::dvec3(0.0);
    return glm::dvec3(glm::transpose(glm::dmat3(sc.frameMat)) * sc.geoRef.ToInternal(lat, lon, alt));
}

// ── rendering ───────────────────────────────────────────────────────

void DrawGlobe(const GlobeConfig& cfg) {
    auto& gr = ctx().geoRef;
    if (!gr.valid) return;

    // Earth center in camera-relative ENU (double precision).
    // earthCenterEnu is stable (constant within the 1.1km GeoRef update threshold).
    glm::dvec3 earthCenterEnu = gr.ecefToEnu * (-gr.ecefRef);
    glm::dvec3 ellCenterD     = earthCenterEnu - glm::dvec3(sCamPos);

    sGlobeShader.Use();
    sGlobeShader.Set("uInvViewProj",  sInvViewProj);
    sGlobeShader.Set("uViewProj",     sViewProj);
    sGlobeShader.Set("uCamPos",       sCamPos);

    // Double-precision ellipsoid — intersection in the shader avoids float32 cancellation
    sGlobeShader.Set("uEllCenterD",   ellCenterD);
    sGlobeShader.Set("uEcefToLocalD", gr.ecefToEnu);
    sGlobeShader.Set("uRadiiD",       glm::dvec3(GeoRef::a, GeoRef::a, GeoRef::b));
    sGlobeShader.Set("uRadii",        glm::vec3(GeoRef::a, GeoRef::a, GeoRef::b));

    // Geodetic params at origin — stable (only update every ~1.1km, not per-frame)
    sGlobeShader.Set("uCamLat",       glm::vec2(float(gr.cosLat), float(gr.sinLat)));
    sGlobeShader.Set("uCurvature",    glm::vec2(float(gr.Nrad), float(gr.Mrad)));

    // Origin lat/lon split: int + fracHi + fracLo → ~1.2e-7° effective ULP in shader
    double latFrac = gr.lat0 - std::floor(gr.lat0);
    double lonFrac = gr.lon0 - std::floor(gr.lon0);
    float  latFHi  = float(latFrac), lonFHi = float(lonFrac);
    sGlobeShader.Set("uCamLLAInt",    glm::vec2(float(std::floor(gr.lat0)), float(std::floor(gr.lon0))));
    sGlobeShader.Set("uCamLLAFracHi", glm::vec2(latFHi, lonFHi));
    sGlobeShader.Set("uCamLLAFracLo", glm::vec2(float(latFrac - double(latFHi)),
                                                  float(lonFrac - double(lonFHi))));

    sGlobeShader.Set("uGratColor",    cfg.gratColor);
    sGlobeShader.Set("uSurfaceColor", cfg.surfaceColor);
    sGlobeShader.Set("uFarPlane",     sFarPlane);

    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(sGlobeVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glEnable(GL_CULL_FACE);
    ++ctx().stats.drawCalls;
}

} // namespace Kilo::Render
