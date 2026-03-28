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

    // Ellipsoid center relative to camera (double → float)
    glm::vec3 ellCenter = glm::vec3(gr.ecefToEnu * (-gr.ecefRef) - glm::dvec3(sCamPos));

    // Origin lat/lon: integer + fractional hi/lo split (double precision in two floats)
    double latFrac = std::fmod(gr.lat0, 1.0), lonFrac = std::fmod(gr.lon0, 1.0);
    float latHi = float(latFrac), lonHi = float(lonFrac);

    sGlobeShader.Use();
    sGlobeShader.Set("uInvViewProj",   sInvViewProj);
    sGlobeShader.Set("uViewProj",      sViewProj);
    sGlobeShader.Set("uCamPos",        sCamPos);
    sGlobeShader.Set("uEllCenter",     ellCenter);
    sGlobeShader.Set("uRadii",         glm::vec3(GeoRef::a, GeoRef::a, GeoRef::b));
    sGlobeShader.Set("uEcefToLocal",   glm::mat3(gr.ecefToEnu));
    sGlobeShader.Set("uOriginInt",     glm::vec2(float(std::floor(gr.lat0)), float(std::floor(gr.lon0))));
    sGlobeShader.Set("uOriginFracHi",  glm::vec2(latHi, lonHi));
    sGlobeShader.Set("uOriginFracLo",  glm::vec2(float(latFrac - double(latHi)), float(lonFrac - double(lonHi))));
    sGlobeShader.Set("uR",             static_cast<float>(GeoRef::a));
    sGlobeShader.Set("uGratColor",     cfg.gratColor);
    sGlobeShader.Set("uSurfaceColor",  cfg.surfaceColor);
    sGlobeShader.Set("uFarPlane",      sFarPlane);

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
