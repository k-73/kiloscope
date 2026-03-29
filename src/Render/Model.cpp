#define TINYOBJLOADER_IMPLEMENTATION
#include "Render/Model.hpp"
#include "Render/DrawState.hpp"
#include "Core/Log.hpp"
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <filesystem>

namespace Kilo::Render {

struct Submesh {
    IndexedMesh mesh;
    glm::vec4   color{.5f, .55f, .6f, 1.f};
};

struct ModelEntry {
    std::vector<Submesh> submeshes;
    float boundingRadius = 0.f;
};

static std::unordered_map<uint32_t, ModelEntry> sModels;

// ── OBJ loader ──────────────────────────────────────────────────────

struct VKey {
    int p, n, t;
    bool operator==(const VKey& o) const { return p == o.p && n == o.n && t == o.t; }
};
struct VKeyHash {
    size_t operator()(const VKey& k) const {
        return size_t(k.p) ^ (size_t(k.n) << 11) ^ (size_t(k.t) << 22);
    }
};

static bool LoadOBJ(const std::string& path, ModelEntry& out) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    auto dir = std::filesystem::path(path).parent_path().string();
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                          path.c_str(), dir.empty() ? nullptr : dir.c_str(), true)) {
        Log::Render().error("Model: failed '{}': {}", path, err);
        return false;
    }
    if (!warn.empty()) Log::Render().warn("Model: {}", warn);

    // Submesh per material (created on demand)
    std::unordered_map<int, int> matMap;
    auto submeshFor = [&](int matId) -> int {
        auto [it, ins] = matMap.try_emplace(matId, int(matMap.size()));
        if (ins) {
            auto& s = out.submeshes.emplace_back();
            if (matId >= 0 && matId < int(materials.size()))
                s.color = {materials[matId].diffuse[0], materials[matId].diffuse[1], materials[matId].diffuse[2], 1.f};
        }
        return it->second;
    };

    std::vector<std::unordered_map<VKey, int, VKeyHash>> vmaps;
    float maxR2 = 0.f;

    for (auto& shape : shapes) {
        size_t off = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            int si = submeshFor(f < shape.mesh.material_ids.size() ? shape.mesh.material_ids[f] : -1);
            if (si >= int(vmaps.size())) vmaps.resize(si + 1);
            auto& sub = out.submeshes[si];
            auto& vm  = vmaps[si];

            int first = -1, prev = -1;
            for (int v = 0; v < fv; ++v) {
                auto& idx = shape.mesh.indices[off + v];
                VKey key{idx.vertex_index, idx.normal_index, idx.texcoord_index};
                auto [it, ins] = vm.try_emplace(key, int(sub.mesh.pos.size()));

                if (ins) {
                    auto* vp = &attrib.vertices[key.p * 3];
                    glm::vec3 p{vp[0], vp[1], vp[2]};
                    sub.mesh.pos.push_back(p);
                    maxR2 = std::max(maxR2, glm::dot(p, p));

                    if (key.n >= 0) { auto* np = &attrib.normals[key.n * 3]; sub.mesh.nrm.push_back(glm::normalize(glm::vec3{np[0], np[1], np[2]})); }
                    else sub.mesh.nrm.push_back({0, 0, 1});

                    if (key.t >= 0) { auto* tp = &attrib.texcoords[key.t * 2]; sub.mesh.uv.push_back({tp[0], tp[1]}); }
                    else sub.mesh.uv.push_back({0, 0});
                }

                int vi = it->second;
                if (v == 0) first = vi;
                else if (v >= 2) sub.mesh.tri.push_back({first, prev, vi});
                prev = vi;
            }
            off += fv;
        }
    }

    // Auto-center: compute AABB center, shift all vertices to origin
    glm::vec3 bmin(FLT_MAX), bmax(-FLT_MAX);
    for (auto& sub : out.submeshes)
        for (auto& p : sub.mesh.pos) { bmin = glm::min(bmin, p); bmax = glm::max(bmax, p); }
    glm::vec3 center = (bmin + bmax) * 0.5f;

    float maxR2c = 0.f;
    for (auto& sub : out.submeshes)
        for (auto& p : sub.mesh.pos) { p -= center; maxR2c = std::max(maxR2c, glm::dot(p, p)); }

    out.boundingRadius = std::sqrt(maxR2c);
    for (auto& sub : out.submeshes) sub.mesh.boundingRadius = out.boundingRadius;
    Log::Render().info("Model: '{}' — {} submeshes, {:.1f}m radius, centered from ({:.1f},{:.1f},{:.1f})",
        path, out.submeshes.size(), out.boundingRadius, center.x, center.y, center.z);
    return true;
}

// ── public API ──────────────────────────────────────────────────────

ModelId LoadModel(const std::string& path) {
    uint32_t id = HashName(path.c_str());
    if (id == 0) id = 1;
    if (sModels.contains(id)) return id;

    ModelEntry entry;
    bool ok = (std::filesystem::path(path).extension() == ".obj") && LoadOBJ(path, entry);
    if (!ok) { Log::Render().error("Model: failed '{}'", path); return kInvalidModel; }

    sModels[id] = std::move(entry);
    return id;
}

bool IsModelLoaded(ModelId id) { return sModels.contains(id); }

void Model(ModelId id, const glm::vec4& color) {
    auto it = sModels.find(id);
    if (it == sModels.end()) return;
    ctx().activePickId = AllocPickId();
    // Cache one-shot flags — apply to all submeshes, not just the first
    bool emissive = ctx().emissive;
    float glowR   = ctx().glowRadius;
    for (auto& sub : it->second.submeshes) {
        SetMeshUniforms(color);
        ctx().twoSided = true;
        if (emissive) { ctx().emissive = true; ctx().glowRadius = glowR; }
        UploadGpuDraw(sub.mesh, Mat());
    }
}

void Model(ModelId id) {
    auto it = sModels.find(id);
    if (it == sModels.end()) return;
    ctx().activePickId = AllocPickId();
    bool emissive = ctx().emissive;
    float glowR   = ctx().glowRadius;
    for (auto& sub : it->second.submeshes) {
        SetMeshUniforms(sub.color);
        ctx().twoSided = true;
        if (emissive) { ctx().emissive = true; ctx().glowRadius = glowR; }
        UploadGpuDraw(sub.mesh, Mat());
    }
}

void ShutdownModels() {
    for (auto& [_, entry] : sModels)
        for (auto& sub : entry.submeshes)
            if (auto& g = sub.mesh.gpu; g.vao) {
                glDeleteVertexArrays(1, &g.vao);
                glDeleteBuffers(1, &g.vbo);
                glDeleteBuffers(1, &g.ebo);
            }
    sModels.clear();
}

} // namespace Kilo::Render
