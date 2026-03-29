#define TINYOBJLOADER_IMPLEMENTATION
#include "Render/Model.hpp"
#include "Render/DrawState.hpp"
#include "Core/Log.hpp"
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <filesystem>

namespace Kilo::Render {

// ── internal types ──────────────────────────────────────────────────

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
        Log::Render().error("Model: failed to load '{}': {}", path, err);
        return false;
    }
    if (!warn.empty()) Log::Render().warn("Model: {}", warn);

    // Group faces by material → submeshes
    // First pass: find unique material IDs across all shapes
    std::unordered_map<int, int> matToSubmesh;

    for (auto& shape : shapes) {
        for (int matId : shape.mesh.material_ids) {
            if (matToSubmesh.find(matId) == matToSubmesh.end()) {
                int idx = static_cast<int>(matToSubmesh.size());
                matToSubmesh[matId] = idx;
            }
        }
    }
    if (matToSubmesh.empty()) matToSubmesh[-1] = 0;

    out.submeshes.resize(matToSubmesh.size());

    // Assign default colors from materials
    for (auto& [matId, subIdx] : matToSubmesh) {
        if (matId >= 0 && matId < static_cast<int>(materials.size())) {
            auto& m = materials[matId];
            out.submeshes[subIdx].color = {m.diffuse[0], m.diffuse[1], m.diffuse[2], 1.f};
        }
    }

    // Per-submesh vertex welding maps
    std::vector<std::unordered_map<VKey, int, VKeyHash>> vertMaps(out.submeshes.size());

    float maxR2 = 0.f;

    for (auto& shape : shapes) {
        size_t indexOff = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            int matId = f < shape.mesh.material_ids.size() ? shape.mesh.material_ids[f] : -1;
            int subIdx = matToSubmesh.count(matId) ? matToSubmesh[matId] : matToSubmesh[-1];
            auto& sub = out.submeshes[subIdx];
            auto& vmap = vertMaps[subIdx];

            // Collect face vertex indices
            std::vector<int> faceIdx(fv);
            for (int v = 0; v < fv; ++v) {
                auto& idx = shape.mesh.indices[indexOff + v];
                VKey key{idx.vertex_index, idx.normal_index, idx.texcoord_index};

                auto it = vmap.find(key);
                if (it != vmap.end()) {
                    faceIdx[v] = it->second;
                } else {
                    int vi = static_cast<int>(sub.mesh.pos.size());
                    vmap[key] = vi;
                    faceIdx[v] = vi;

                    glm::vec3 p{0};
                    if (key.p >= 0 && key.p * 3 + 2 < static_cast<int>(attrib.vertices.size()))
                        p = {attrib.vertices[key.p*3], attrib.vertices[key.p*3+1], attrib.vertices[key.p*3+2]};
                    sub.mesh.pos.push_back(p);
                    maxR2 = std::max(maxR2, glm::dot(p, p));

                    glm::vec3 n{0, 0, 1};
                    if (key.n >= 0 && key.n * 3 + 2 < static_cast<int>(attrib.normals.size()))
                        n = glm::normalize(glm::vec3{attrib.normals[key.n*3], attrib.normals[key.n*3+1], attrib.normals[key.n*3+2]});
                    sub.mesh.nrm.push_back(n);

                    glm::vec2 uv{0};
                    if (key.t >= 0 && key.t * 2 + 1 < static_cast<int>(attrib.texcoords.size()))
                        uv = {attrib.texcoords[key.t*2], attrib.texcoords[key.t*2+1]};
                    sub.mesh.uv.push_back(uv);
                }
            }

            // Triangulate (fan from first vertex)
            for (int v = 1; v + 1 < fv; ++v)
                sub.mesh.tri.push_back({faceIdx[0], faceIdx[v], faceIdx[v+1]});

            indexOff += fv;
        }
    }

    // Compute bounding radius per submesh and overall
    out.boundingRadius = std::sqrt(maxR2);
    for (auto& sub : out.submeshes)
        sub.mesh.boundingRadius = out.boundingRadius;

    Log::Render().info("Model: loaded '{}' — {} submeshes, {:.1f}m radius",
        path, out.submeshes.size(), out.boundingRadius);
    return true;
}

// ── public API ──────────────────────────────────────────────────────

ModelId LoadModel(const std::string& path) {
    uint32_t id = HashName(path.c_str());
    if (id == 0) id = 1;  // avoid kInvalidModel
    if (sModels.count(id)) return id;

    auto ext = std::filesystem::path(path).extension().string();
    ModelEntry entry;
    bool ok = false;

    if (ext == ".obj")
        ok = LoadOBJ(path, entry);
    else
        Log::Render().error("Model: unsupported format '{}'", ext);

    if (!ok) return kInvalidModel;
    sModels[id] = std::move(entry);
    return id;
}

bool IsModelLoaded(ModelId id) {
    return sModels.count(id) > 0;
}

void Model(ModelId id, const glm::vec4& color) {
    auto it = sModels.find(id);
    if (it == sModels.end()) return;
    ctx().activePickId = AllocPickId();
    SetMeshUniforms(color);
    for (auto& sub : it->second.submeshes)
        UploadGpuDraw(sub.mesh, Mat());
}

void Model(ModelId id) {
    auto it = sModels.find(id);
    if (it == sModels.end()) return;
    ctx().activePickId = AllocPickId();
    for (auto& sub : it->second.submeshes) {
        SetMeshUniforms(sub.color);
        UploadGpuDraw(sub.mesh, Mat());
    }
}

void ShutdownModels() {
    for (auto& [_, entry] : sModels)
        for (auto& sub : entry.submeshes) {
            auto& g = sub.mesh.gpu;
            if (g.vao) {
                glDeleteVertexArrays(1, &g.vao);
                glDeleteBuffers(1, &g.vbo);
                glDeleteBuffers(1, &g.ebo);
                g.vao = g.vbo = g.ebo = 0;
            }
        }
    sModels.clear();
}

} // namespace Kilo::Render
