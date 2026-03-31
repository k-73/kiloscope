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

glm::dvec3 EcefToLocal(const char* name, const glm::dvec3& ecef) {
    auto& sc = GetScene(name);
    if (!sc.geoRef.valid) return glm::dvec3(0.0);
    return glm::dvec3(glm::transpose(glm::dmat3(sc.frameMat)) * sc.geoRef.EcefToInternal(ecef));
}

glm::dvec3 EcefToLocal(const glm::dvec3& ecef) {
    auto& gr = ctx().geoRef;
    if (!gr.valid) return glm::dvec3(0.0);
    return glm::dvec3(glm::transpose(glm::dmat3(ctx().frameMat)) * gr.EcefToInternal(ecef));
}

// ── screen-to-geo (CPU ray-ellipsoid intersection) ─────────────────
//
// Pipeline: screen → NDC → camera-relative world ray → ENU world ray
//           → ECEF normalized → quadratic → hit ECEF → geodetic (GeographicLib)
//
// Same math as Globe.frag but on CPU with full double precision.

static bool ScreenToGeoImpl(const SceneData& sc,
                            float screenX, float screenY,
                            double& lat, double& lon, double& alt) {
    auto& gr = sc.geoRef;
    if (!gr.valid || sc.cachedVpW < 1.f) return false;

    // 1. Screen → NDC (using per-scene cached viewport)
    float ndcX = (screenX - sc.cachedVpCx) / sc.cachedVpW *  2.f - 1.f;
    float ndcY = (1.f - (screenY - sc.cachedVpCy) / sc.cachedVpH) * 2.f - 1.f;
    if (ndcX < -1.f || ndcX > 1.f || ndcY < -1.f || ndcY > 1.f) return false;

    // 2. Unproject screen point onto near plane (camera-relative world space).
    //    Camera is at origin in camera-relative space, so ray direction = normalize(nearP).
    //    No far-plane unproject needed — avoids float32 precision loss at extreme far distances.
    glm::vec4 nearH = sc.cachedInvViewProj * glm::vec4(ndcX, ndcY, -1.f, 1.f);
    glm::dvec3 nearP = glm::dvec3(nearH) / double(nearH.w);
    glm::dvec3 rd    = glm::normalize(nearP);

    // 3. Ray origin = camera position in ENU (relative to GeoRef origin)
    glm::dvec3 rayOriginEnu = sc.cachedCamPosD;

    // 4. Transform ray to ECEF normalized space (unit ellipsoid)
    constexpr double a = GeoRef::a, b = GeoRef::b;
    glm::dvec3 invR(1.0 / a, 1.0 / a, 1.0 / b);
    glm::dmat3 toEcef = glm::transpose(gr.ecefToEnu);
    glm::dmat3 M(toEcef[0] * invR, toEcef[1] * invR, toEcef[2] * invR);

    // Earth center in ENU = ecefToEnu * (-ecefRef)
    glm::dvec3 earthCenterEnu = gr.ecefToEnu * (-gr.ecefRef);
    glm::dvec3 oc = M * (rayOriginEnu - earthCenterEnu);
    // M * v = diag(1/radii) * transpose(ecefToEnu)^-1 * v = normalized ECEF direction
    glm::dvec3 dd = M * rd;

    // 5. Solve quadratic: |oc + t*dd|² = 1
    //    Standard formula with double precision — no GPU near-root trick needed.
    double qa   = glm::dot(dd, dd);
    double qb   = glm::dot(oc, dd);
    double qc_  = glm::dot(oc, oc) - 1.0;
    double disc = qb * qb - qa * qc_;
    if (disc < 0.0) return false;

    double sd = std::sqrt(disc);
    double t1 = (-qb - sd) / qa;   // near root
    double t2 = (-qb + sd) / qa;   // far root
    double tD = (t1 > 0.0) ? t1 : t2;

    if (tD < 0.0 || !std::isfinite(tD)) return false;

    // 6. Hit point in ENU → ECEF → geodetic
    glm::dvec3 hitEnu  = rayOriginEnu + rd * tD;
    glm::dvec3 hitEcef = toEcef * hitEnu + gr.ecefRef;

    if (!std::isfinite(hitEcef.x)) return false;

    static const auto& earth = GeographicLib::Geocentric::WGS84();
    earth.Reverse(hitEcef.x, hitEcef.y, hitEcef.z, lat, lon, alt);

    return std::isfinite(lat) && std::isfinite(lon);
}

bool ScreenToGeo(float screenX, float screenY, double& lat, double& lon, double& alt) {
    if (!sFrame.scene) return false;
    return ScreenToGeoImpl(*sFrame.scene, screenX, screenY, lat, lon, alt);
}

bool ScreenToGeo(const char* scene, float screenX, float screenY,
                 double& lat, double& lon, double& alt) {
    return ScreenToGeoImpl(GetScene(scene), screenX, screenY, lat, lon, alt);
}

// ── rendering ───────────────────────────────────────────────────────

void DrawGlobe(const GlobeConfig& cfg) {
    auto& gr = ctx().geoRef;
    if (!gr.valid) return;

    // Earth center in camera-relative ENU (double).
    // earthCenterEnu is stable — only changes at the 1.1km GeoRef update threshold.
    glm::dvec3 earthCenterEnu = gr.ecefToEnu * (-gr.ecefRef);
    glm::dvec3 ellCenterD     = earthCenterEnu - sCamPosD;

    sGlobeShader.Use();
    sGlobeShader.Set("uInvViewProj",  sInvViewProj);
    sGlobeShader.Set("uViewProj",     sViewProj);
    sGlobeShader.Set("uCamPos",       sCamPos);

    // Combined matrix: transpose(ecefToEnu) * diag(1/radii).
    // Folds the constant radii division into the rotation — avoids 6 double divides per fragment.
    glm::dvec3 invRadii(1.0 / GeoRef::a, 1.0 / GeoRef::a, 1.0 / GeoRef::b);
    glm::dmat3 toEcef = glm::transpose(gr.ecefToEnu);
    glm::dmat3 toEcefNorm(toEcef[0] * invRadii, toEcef[1] * invRadii, toEcef[2] * invRadii);

    sGlobeShader.Set("uEllCenterD",   ellCenterD);
    sGlobeShader.Set("uToEcefNorm",   toEcefNorm);
    sGlobeShader.Set("uEcefToLocalD", gr.ecefToEnu);
    sGlobeShader.Set("uRadiiD",       glm::dvec3(GeoRef::a, GeoRef::a, GeoRef::b));
    sGlobeShader.Set("uRadii",        glm::vec3(GeoRef::a, GeoRef::a, GeoRef::b));

    // Geodetic params at origin — updated every frame
    sGlobeShader.Set("uOriginLat",    glm::vec2(float(gr.cosLat), float(gr.sinLat)));
    sGlobeShader.Set("uOriginNM",     glm::vec2(float(gr.Nrad), float(gr.Mrad)));

    // Origin lat/lon for the shader: must use refLat/refLon (the frozen reference),
    // NOT lat0/lon0 (which update every frame).  dLon/dLat in the shader are deltas
    // from the frozen ENU origin — adding them to refLat/refLon gives correct absolute coords.
    double rlat = gr.refLat(), rlon = gr.refLon();
    double latFrac = rlat - std::floor(rlat);
    double lonFrac = rlon - std::floor(rlon);
    float  latFHi  = float(latFrac), lonFHi = float(lonFrac);
    sGlobeShader.Set("uOriginLLInt",    glm::vec2(float(std::floor(rlon)), float(std::floor(rlat))));
    sGlobeShader.Set("uOriginLLFracHi", glm::vec2(lonFHi, latFHi));
    sGlobeShader.Set("uOriginLLFracLo", glm::vec2(float(lonFrac - double(lonFHi)),
                                                    float(latFrac - double(latFHi))));

    sGlobeShader.Set("uGratColor",    cfg.gratColor);
    sGlobeShader.Set("uSurfaceColor", cfg.surfaceColor);
    sGlobeShader.Set("uLightDir",     sLightDir);
    sGlobeShader.Set("uAmbient",      cfg.lighting ? cfg.ambient : 1.f);
    sGlobeShader.Set("uAtmoColor",    cfg.atmosphereColor);
    sGlobeShader.Set("uAtmoParams",   glm::vec2(cfg.atmospherePow, cfg.atmosphereStr));
    sGlobeShader.Set("uFogColor",     cfg.fogColor);
    sGlobeShader.Set("uFogParams",    cfg.fog ? glm::vec2(cfg.fogStart, cfg.fogEnd) : glm::vec2(1e9f, 2e9f));
    sGlobeShader.Set("uGridFades",    cfg.gridFades);
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
