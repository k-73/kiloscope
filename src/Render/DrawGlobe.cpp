#include "Render/DrawGlobe.hpp"
#include "Render/DrawState.hpp"
#include "Core/Log.hpp"
#include <cstring>
#include <filesystem>
#include <limits>
#include <tiffio.h>
#include <GeographicLib/Geoid.hpp>

namespace Kilo::Render {

static Shader sGlobeShader;
static Shader sTerrainShader;
static GLuint sGlobeVao = 0;

// ── terrain ─────────────────────────────────────────────────────────

float TerrainTile::Sample(double lat, double lon) const {
    if (elevation.empty()) return 0.f;
    float u = float((lon - lonMin) / (lonMax - lonMin));
    float v = float((lat - latMin) / (latMax - latMin));
    if (u < 0.f || u > 1.f || v < 0.f || v > 1.f) return 0.f;
    float fx = u * (cols - 1), fy = v * (rows - 1);
    int x0 = std::min(int(fx), cols - 2), y0 = std::min(int(fy), rows - 2);
    float sx = fx - x0, sy = fy - y0;
    float e00 = elevation[y0 * cols + x0],       e10 = elevation[y0 * cols + x0 + 1];
    float e01 = elevation[(y0 + 1) * cols + x0], e11 = elevation[(y0 + 1) * cols + x0 + 1];
    return (e00 * (1 - sx) + e10 * sx) * (1 - sy) + (e01 * (1 - sx) + e11 * sx) * sy;
}

// ── GeoTIFF reader ──────────────────────────────────────────────────

// Suppress libtiff warnings about unknown GeoTIFF tags (33550, 33922, 34735-37).
static void TiffWarningHandler(const char*, const char*, va_list) {}

static constexpr uint16_t kGeoPixelScaleTag = 33550;
static constexpr uint16_t kGeoTiepointTag   = 33922;
static constexpr float    kNoData           = -32767.f;

static TerrainTile LoadGeoTiff(const std::string& path) {
    TerrainTile tile{};
    auto* prevWarn = TIFFSetWarningHandler(TiffWarningHandler);
    TIFF* tif = TIFFOpen(path.c_str(), "r");
    auto restore = [&] { TIFFSetWarningHandler(prevWarn); };
    if (!tif) { Log::Render().error("GeoTIFF not found: {}", path); restore(); return tile; }

    uint32_t w = 0, h = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    if (!w || !h) { Log::Render().error("GeoTIFF bad dims: {}", path); TIFFClose(tif); restore(); return tile; }

    // Geo metadata: pixel scale + tiepoint → bounds
    uint32_t cnt = 0;
    double* scale = nullptr;
    double* tp    = nullptr;
    TIFFGetField(tif, kGeoPixelScaleTag, &cnt, &scale);
    TIFFGetField(tif, kGeoTiepointTag,   &cnt, &tp);
    if (!scale || !tp) {
        Log::Render().error("GeoTIFF missing geo tags: {}", path);
        TIFFClose(tif); restore(); return tile;
    }
    double lonMin = tp[3] - tp[0] * scale[0];
    double latMax = tp[4] + tp[1] * scale[1];
    double lonMax = lonMin + w * scale[0];
    double latMin = latMax - h * scale[1];

    // Sample format (Float32 or Int16)
    uint16_t bps = 0, sf = SAMPLEFORMAT_UINT;
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bps);
    TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &sf);
    bool isFloat = (sf == SAMPLEFORMAT_IEEEFP && bps == 32);
    bool isInt16 = (sf == SAMPLEFORMAT_INT && bps == 16);
    if (!isFloat && !isInt16) {
        Log::Render().error("GeoTIFF unsupported format (bps={} sf={}): {}", bps, sf, path);
        TIFFClose(tif); restore(); return tile;
    }

    // Read raster — TIFF stores top-down, TerrainTile needs bottom-up
    tile.elevation.resize(w * h);
    tile.cols = int(w); tile.rows = int(h);

    // Convert one scanline of raw bytes into float elevation, writing to dst[0..count).
    auto decodeLine = [&](const uint8_t* src, float* dst, uint32_t count) {
        if (isFloat)
            std::memcpy(dst, src, count * sizeof(float));
        else
            for (uint32_t i = 0; i < count; ++i)
                dst[i] = float(reinterpret_cast<const int16_t*>(src)[i]);
    };

    if (TIFFIsTiled(tif)) {
        uint32_t tw = 0, th = 0;
        TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);
        TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);
        std::vector<uint8_t> buf(TIFFTileSize(tif));
        for (uint32_t y = 0; y < h; y += th)
            for (uint32_t x = 0; x < w; x += tw) {
                if (TIFFReadEncodedTile(tif, TIFFComputeTile(tif, x, y, 0, 0),
                                        buf.data(), buf.size()) < 0) continue;
                uint32_t copyW = std::min(tw, w - x);
                uint32_t bpp   = bps / 8;
                for (uint32_t ty = 0; ty < th && y + ty < h; ++ty) {
                    uint32_t dstRow = (h - 1) - (y + ty);
                    decodeLine(&buf[ty * tw * bpp], &tile.elevation[dstRow * w + x], copyW);
                }
            }
    } else {
        std::vector<uint8_t> buf(TIFFScanlineSize(tif));
        for (uint32_t y = 0; y < h; ++y) {
            if (TIFFReadScanline(tif, buf.data(), y) < 0) continue;
            uint32_t dstRow = (h - 1) - y;
            decodeLine(buf.data(), &tile.elevation[dstRow * w], w);
        }
    }
    TIFFClose(tif);
    restore();

    tile.lonMin = float(lonMin); tile.latMin = float(latMin);
    tile.lonMax = float(lonMax); tile.latMax = float(latMax);

    // Compute elevation range, treating nodata as missing
    tile.elevMin = std::numeric_limits<float>::max();
    tile.elevMax = std::numeric_limits<float>::lowest();
    for (float& v : tile.elevation) {
        if (v <= kNoData) { v = 0.f; continue; }
        tile.elevMin = std::min(tile.elevMin, v);
        tile.elevMax = std::max(tile.elevMax, v);
    }
    if (tile.elevMin > tile.elevMax) tile.elevMin = tile.elevMax = 0.f;  // all nodata

    Log::Render().info("GeoTIFF: {}x{} [{:.4f},{:.4f}]→[{:.4f},{:.4f}] elev [{:.0f},{:.0f}]m",
                       w, h, lonMin, latMin, lonMax, latMax, tile.elevMin, tile.elevMax);
    return tile;
}

TerrainTile LoadTerrain(const std::string& path) {
    auto ext = path.substr(path.find_last_of('.') + 1);
    if (ext == "tif" || ext == "tiff") return LoadGeoTiff(path);
    Log::Render().error("LoadTerrain: unsupported format '{}' (use .tif or explicit params): {}", ext, path);
    return {};
}

// ── TerrainSet (multi-tile) ─────────────────────────────────────

void TerrainSet::BuildIndex() {
    index.clear();
    for (size_t i = 0; i < tiles.size(); ++i) {
        auto& t = tiles[i];
        int latI = int(std::floor(t.latMin));
        int lonI = int(std::floor(t.lonMin));
        index[TileKey(latI, lonI)] = i;
    }
}

float TerrainSet::Sample(double lat, double lon) const {
    auto it = index.find(TileKey(int(std::floor(lat)), int(std::floor(lon))));
    if (it != index.end()) return tiles[it->second].Sample(lat, lon);
    return 0.f;
}

TerrainSet LoadTerrainDir(const std::string& dir, const std::string& geoidPath) {
    TerrainSet set;
    if (!std::filesystem::is_directory(dir)) {
        Log::Render().error("Terrain directory not found: {}", dir);
        return set;
    }

    // Optional geoid correction: convert EGM geoid heights → WGS84 ellipsoidal heights
    std::unique_ptr<GeographicLib::Geoid> geoid;
    if (!geoidPath.empty()) {
        try {
            // Geoid() wants dir + name without extension, e.g. ("assets/geoid", "egm2008-5")
            auto gp = std::filesystem::path(geoidPath);
            geoid = std::make_unique<GeographicLib::Geoid>(gp.stem().string(), gp.parent_path().string());
            Log::Render().info("Geoid loaded: {}", geoidPath);
        } catch (const std::exception& e) {
            Log::Render().warn("Geoid not available ({}), loading without correction", e.what());
        }
    }

    std::vector<std::filesystem::path> paths;
    for (auto& e : std::filesystem::directory_iterator(dir))
        if (e.is_regular_file() && (e.path().extension() == ".tif" || e.path().extension() == ".tiff"))
            paths.push_back(e.path());
    std::sort(paths.begin(), paths.end());

    set.elevMin = std::numeric_limits<float>::max();
    set.elevMax = std::numeric_limits<float>::lowest();
    set.lonMin  = 999.f; set.latMin = 999.f;
    set.lonMax  = -999.f; set.latMax = -999.f;

    for (auto& p : paths) {
        auto tile = LoadTerrain(p.string());
        if (tile.elevation.empty()) continue;

        // Apply geoid undulation — bilinear interpolation from 4 corners (~0.1m accuracy)
        if (geoid) {
            float uSW = float((*geoid)(tile.latMin, tile.lonMin));
            float uSE = float((*geoid)(tile.latMin, tile.lonMax));
            float uNW = float((*geoid)(tile.latMax, tile.lonMin));
            float uNE = float((*geoid)(tile.latMax, tile.lonMax));
            for (int y = 0; y < tile.rows; ++y) {
                float v = float(y) / float(tile.rows - 1);  // 0=south, 1=north
                float uW = uSW + (uNW - uSW) * v;
                float uE = uSE + (uNE - uSE) * v;
                for (int x = 0; x < tile.cols; ++x) {
                    float u = float(x) / float(tile.cols - 1);
                    tile.elevation[y * tile.cols + x] += uW + (uE - uW) * u;
                }
            }
            float uMin = std::min({uSW, uSE, uNW, uNE});
            float uMax = std::max({uSW, uSE, uNW, uNE});
            tile.elevMin += uMin;
            tile.elevMax += uMax;
        }

        set.lonMin  = std::min(set.lonMin,  tile.lonMin);
        set.latMin  = std::min(set.latMin,  tile.latMin);
        set.lonMax  = std::max(set.lonMax,  tile.lonMax);
        set.latMax  = std::max(set.latMax,  tile.latMax);
        set.elevMin = std::min(set.elevMin, tile.elevMin);
        set.elevMax = std::max(set.elevMax, tile.elevMax);
        set.tiles.push_back(std::move(tile));
    }

    set.BuildIndex();
    if (set.tiles.empty()) {
        Log::Render().error("No terrain tiles in: {}", dir);
        set.elevMin = set.elevMax = 0.f;
    } else {
        Log::Render().info("TerrainSet: {} tiles [{:.1f},{:.1f}]→[{:.1f},{:.1f}] elev [{:.0f},{:.0f}]m{}",
                           set.tiles.size(), set.lonMin, set.latMin, set.lonMax, set.latMax,
                           set.elevMin, set.elevMax, geoid ? " (geoid corrected)" : "");
    }
    return set;
}

// ── TerrainMesh GPU ─────────────────────────────────────────────────

TerrainMesh::TerrainMesh(TerrainMesh&& o) noexcept
    : ecefCenter(o.ecefCenter), relPos(std::move(o.relPos)), normals(std::move(o.normals)),
      colors(std::move(o.colors)), indices(std::move(o.indices)),
      vao(o.vao), vbo(o.vbo), ebo(o.ebo), indexCount(o.indexCount) {
    o.vao = o.vbo = o.ebo = 0; o.indexCount = 0;
}

TerrainMesh& TerrainMesh::operator=(TerrainMesh&& o) noexcept {
    if (this != &o) { Destroy(); new (this) TerrainMesh(std::move(o)); }
    return *this;
}

void TerrainMesh::Upload() {
    if (vao || indices.empty()) return;

    // Interleaved: pos(3f) + normal(3f) + color(4f) = 40 bytes/vertex
    struct Vert { glm::vec3 pos, nrm; glm::vec4 col; };
    std::vector<Vert> verts(relPos.size());
    for (size_t i = 0; i < relPos.size(); ++i)
        verts[i] = {relPos[i], normals[i], colors[i]};

    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);
    glCreateBuffers(1, &ebo);
    glNamedBufferStorage(vbo, GLsizeiptr(verts.size() * sizeof(Vert)), verts.data(), 0);
    glNamedBufferStorage(ebo, GLsizeiptr(indices.size() * sizeof(uint32_t)), indices.data(), 0);

    auto attr = [&](GLuint idx, GLint size, GLenum type, GLuint offset) {
        glEnableVertexArrayAttrib(vao, idx);
        glVertexArrayAttribFormat(vao, idx, size, type, GL_FALSE, offset);
        glVertexArrayAttribBinding(vao, idx, 0);
    };
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(Vert));
    glVertexArrayElementBuffer(vao, ebo);
    attr(0, 3, GL_FLOAT, offsetof(Vert, pos));
    attr(1, 3, GL_FLOAT, offsetof(Vert, nrm));
    attr(2, 4, GL_FLOAT, offsetof(Vert, col));

    indexCount = static_cast<int>(indices.size());
    Log::Render().info("TerrainMesh GPU: {} verts, {} tris", relPos.size(), indexCount / 3);
}

void TerrainMesh::Destroy() {
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    indexCount = 0;
}

TerrainMesh BuildTerrainMesh(const TerrainSet& terrain, double centerLat, double centerLon,
                             float latRadDeg, float lonRadDeg, float stepDeg) {
    TerrainMesh mesh;
    if (terrain.empty()) return mesh;

    float lat0 = std::max(float(centerLat - latRadDeg), terrain.latMin);
    float lat1 = std::min(float(centerLat + latRadDeg), terrain.latMax);
    float lon0 = std::max(float(centerLon - lonRadDeg), terrain.lonMin);
    float lon1 = std::min(float(centerLon + lonRadDeg), terrain.lonMax);
    if (lat0 >= lat1 || lon0 >= lon1) return mesh;

    int nx = std::max(2, int((lon1 - lon0) / stepDeg) + 1);
    int ny = std::max(2, int((lat1 - lat0) / stepDeg) + 1);
    float dLon = (lon1 - lon0) / (nx - 1);
    float dLat = (lat1 - lat0) / (ny - 1);

    // ECEF reference = center of mesh (for float32 relative positions)
    mesh.ecefCenter = GeoRef::ToEcef(centerLat, centerLon, 0.0);

    int nv = nx * ny;
    mesh.relPos.resize(nv);
    mesh.normals.resize(nv);
    mesh.colors.resize(nv);

    // Precompute lon trig (constant per column) — avoids redundant sin/cos per vertex
    std::vector<double> cosLon(nx), sinLon(nx);
    for (int ix = 0; ix < nx; ++ix) {
        double lonR = glm::radians(double(lon0 + ix * dLon));
        cosLon[ix] = std::cos(lonR);
        sinLon[ix] = std::sin(lonR);
    }

    // Vertex positions: inline ECEF with per-row trig caching
    mesh.elevMin = std::numeric_limits<float>::max();
    mesh.elevMax = std::numeric_limits<float>::lowest();
    std::vector<glm::dvec3> ecefFull(nv);
    for (int iy = 0; iy < ny; ++iy) {
        double lat = lat0 + iy * dLat;
        double latR = glm::radians(lat);
        double sLat = std::sin(latR), cLat = std::cos(latR);
        double N = GeoRef::a / std::sqrt(1.0 - GeoRef::e2 * sLat * sLat);
        double Nz = N * (1.0 - GeoRef::e2);

        for (int ix = 0; ix < nx; ++ix) {
            float  elev = terrain.Sample(lat, lon0 + ix * dLon);
            double alt  = double(elev);
            int    idx  = iy * nx + ix;
            ecefFull[idx] = {(N + alt) * cLat * cosLon[ix],
                             (N + alt) * cLat * sinLon[ix],
                             (Nz + alt) * sLat};
            mesh.relPos[idx] = glm::vec3(ecefFull[idx] - mesh.ecefCenter);
            mesh.colors[idx] = glm::vec4(elev, 0.f, 0.f, 1.f);
            mesh.elevMin = std::min(mesh.elevMin, elev);
            mesh.elevMax = std::max(mesh.elevMax, elev);
        }
    }
    if (mesh.elevMin > mesh.elevMax) mesh.elevMin = mesh.elevMax = 0.f;

    // Normals + slope (combined pass)
    for (int iy = 0; iy < ny; ++iy)
        for (int ix = 0; ix < nx; ++ix) {
            int idx = iy * nx + ix;
            int xr = std::min(ix + 1, nx - 1), xl = std::max(ix - 1, 0);
            int yu = std::min(iy + 1, ny - 1), yd = std::max(iy - 1, 0);
            glm::dvec3 dEast  = ecefFull[iy * nx + xr] - ecefFull[iy * nx + xl];
            glm::dvec3 dNorth = ecefFull[yu * nx + ix]  - ecefFull[yd * nx + ix];
            glm::dvec3 n = glm::cross(dEast, dNorth);
            if (glm::dot(n, ecefFull[idx]) < 0.0) n = -n;
            glm::dvec3 nn = glm::normalize(n);
            mesh.normals[idx] = glm::vec3(nn);
            glm::dvec3 radial = glm::normalize(ecefFull[idx]);
            mesh.colors[idx].y = float(1.0 - std::abs(glm::dot(nn, radial)));
        }

    // Triangle indices (direct write, no per-quad allocation)
    mesh.indices.resize(size_t(nx - 1) * (ny - 1) * 6);
    uint32_t* idx = mesh.indices.data();
    for (int iy = 0; iy + 1 < ny; ++iy)
        for (int ix = 0; ix + 1 < nx; ++ix) {
            uint32_t a = iy * nx + ix, b = a + 1, c = a + nx, d = c + 1;
            *idx++ = a; *idx++ = b; *idx++ = c;
            *idx++ = b; *idx++ = d; *idx++ = c;
        }

    return mesh;
}

// Called by user code inside Begin/End — defers rendering to End() (after Globe).
void DrawTerrain(TerrainMesh& mesh, float globalElevMin, float globalElevMax) {
    if (mesh.indices.empty()) return;
    mesh.Upload();
    ctx().pendingTerrain = &mesh;
    ctx().globalElevMin = globalElevMin;
    ctx().globalElevMax = globalElevMax;
}

// Called from End() after Globe/Grid — terrain renders on top of globe surface.
void RenderTerrain() {
    auto* mesh = ctx().pendingTerrain;
    ctx().pendingTerrain = nullptr;
    if (!mesh || !mesh->vao) return;

    auto& gr = ctx().geoRef;
    if (!gr.valid) return;

    glm::mat3 ecefToEnu = glm::mat3(gr.ecefToEnu);
    float fcoef = 2.f / std::log2(sFarPlane + 1.f);

    sTerrainShader.Use();
    sTerrainShader.Set("uEcefToEnu",  ecefToEnu);
    sTerrainShader.Set("uMeshOffset", glm::vec3(mesh->ecefCenter - gr.ecefRef));
    sTerrainShader.Set("uCamEnu",     glm::vec3(sCamPosD));
    sTerrainShader.Set("uViewProj",   sViewProj);
    sTerrainShader.Set("uFcoef",      fcoef);
    sTerrainShader.Set("uLightDir",   sLightDir);
    sTerrainShader.Set("uAmbient",    ctx().env.ambient);
    sTerrainShader.Set("uFogColor",   ctx().env.fogColor);
    sTerrainShader.Set("uFogStart",   ctx().env.fogStart);
    sTerrainShader.Set("uFogEnd",     ctx().env.fogEnd);
    sTerrainShader.Set("uFcoefHalf",  fcoef * 0.5f);
    sTerrainShader.Set("uElevMin",    ctx().globalElevMin);
    sTerrainShader.Set("uElevMax",    ctx().globalElevMax);
    sTerrainShader.Set("uLocalMin",   mesh->elevMin);
    sTerrainShader.Set("uLocalMax",   mesh->elevMax);

    glDisable(GL_CULL_FACE);
    glBindVertexArray(mesh->vao);
    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, nullptr);
    glEnable(GL_CULL_FACE);
    ++ctx().stats.drawCalls;
    ctx().stats.vertices += mesh->indexCount;
}

// ── init / shutdown ─────────────────────────────────────────────────

void InitGlobe(const std::string& dir) {
    sGlobeShader   = Shader(dir + "/Globe.vert",   dir + "/Globe.frag");
    sTerrainShader = Shader(dir + "/Terrain.vert",  dir + "/Terrain.frag");
    glCreateVertexArrays(1, &sGlobeVao);
}

void ShutdownGlobe() {
    sGlobeShader = {};
    sTerrainShader = {};
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
                            double& lat, double& lon, double& alt,
                            double altOffset = 0.0) {
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

    // 4. Transform ray to ECEF normalized space (unit ellipsoid, optionally inflated by altOffset)
    double ra = GeoRef::a + altOffset, rb = GeoRef::b + altOffset;
    glm::dvec3 invR(1.0 / ra, 1.0 / ra, 1.0 / rb);
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

bool ScreenToGeo(float screenX, float screenY, double& lat, double& lon, double& alt,
                 double altOffset) {
    if (!sFrame.scene) return false;
    return ScreenToGeoImpl(*sFrame.scene, screenX, screenY, lat, lon, alt, altOffset);
}

bool ScreenToGeo(const char* scene, float screenX, float screenY,
                 double& lat, double& lon, double& alt, double altOffset) {
    return ScreenToGeoImpl(GetScene(scene), screenX, screenY, lat, lon, alt, altOffset);
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
