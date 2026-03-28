#include "Render/DrawGlobe.hpp"
#include "Render/DrawState.hpp"

namespace Kilo::Render {

// Globe-owned GPU resources (not shared — only used here)
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
    auto enu = gr.ToInternal(lat, lon, alt);
    return glm::dvec3(glm::transpose(glm::dmat3(ctx().frameMat)) * enu);
}

glm::dvec3 GeoToLocal(const char* name, double lat, double lon, double alt) {
    auto& sc = GetScene(name);
    if (!sc.geoRef.valid) return glm::dvec3(0.0);
    auto enu = sc.geoRef.ToInternal(lat, lon, alt);
    return glm::dvec3(glm::transpose(glm::dmat3(sc.frameMat)) * enu);
}

// ── rendering ───────────────────────────────────────────────────────

// Diagnostics (accessible from panels)
static glm::vec3 sDbgEllCenter{0};
static float sDbgFarPlane = 0;
static float sDbgCamZ = 0;
glm::vec3 GlobeDbgEllCenter() { return sDbgEllCenter; }
float GlobeDbgFarPlane() { return sDbgFarPlane; }
float GlobeDbgCamZ() { return sDbgCamZ; }

void DrawGlobe(const GlobeConfig& cfg) {
    auto& gr = ctx().geoRef;
    if (!gr.valid) return;

    // Ellipsoid center position in world ENU, relative to camera
    // Earth center in ENU = ecefToEnu * (0 - ecefRef) = ecefToEnu * (-ecefRef)
    // Then subtract camera position (sCamPos is already in world ENU)
    // Ellipsoid center relative to camera (full double precision → GPU)
    glm::dvec3 ellCenterD = gr.ecefToEnu * (-gr.ecefRef) - glm::dvec3(sCamPos);

    sDbgEllCenter = glm::vec3(ellCenterD);
    sDbgFarPlane = sFarPlane;
    sDbgCamZ = sCamPos.z;

    sGlobeShader.Use();
    sGlobeShader.Set("uInvViewProj",  sInvViewProj);
    sGlobeShader.Set("uViewProj",     sViewProj);
    sGlobeShader.Set("uCamPos",       sCamPos);
    sGlobeShader.Set("uEllCenter",    glm::vec3(ellCenterD));
    sGlobeShader.Set("uRadii",        glm::vec3(GeoRef::a, GeoRef::a, GeoRef::b));
    sGlobeShader.Set("uEcefToLocal",  glm::mat3(gr.ecefToEnu));
    // Origin: integer degrees + fractional degrees (hi/lo split of fractional part)
    // Fine grids (< 1°) use only fractional → full float32 precision in 0-1 range
    double latFrac = std::fmod(gr.lat0, 1.0), lonFrac = std::fmod(gr.lon0, 1.0);
    float latFracHi = float(latFrac), lonFracHi = float(lonFrac);
    float latFracLo = float(latFrac - double(latFracHi));
    float lonFracLo = float(lonFrac - double(lonFracHi));
    sGlobeShader.Set("uOriginInt",     glm::vec2(float(std::floor(gr.lat0)), float(std::floor(gr.lon0))));
    sGlobeShader.Set("uOriginFracHi",  glm::vec2(latFracHi, lonFracHi));
    sGlobeShader.Set("uOriginFracLo",  glm::vec2(latFracLo, lonFracLo));
    sGlobeShader.Set("uR",            static_cast<float>(GeoRef::a));
    sGlobeShader.Set("uGratColor",    cfg.gratColor);
    sGlobeShader.Set("uSurfaceColor", cfg.surfaceColor);
    sGlobeShader.Set("uFarPlane",     sFarPlane);
    sGlobeShader.Set("uCamDist",     ctx().cam.Distance());

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
