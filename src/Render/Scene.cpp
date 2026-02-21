#include "Scene.hpp"

namespace KiloScope::Render {

void Scene::Init(const std::string& dir) {
    grid_.Init(dir);
    prims_.Init(dir);
}

void Scene::BeginRender() {
    fbo_.Bind();
    int w = fbo_.Width(), h = fbo_.Height();
    float aspect = static_cast<float>(w) / std::max(1, h);
    auto view = cam_.View();
    auto proj = cam_.Projection(aspect);
    auto pos  = cam_.Position();

    prims_.Begin(view, proj, pos, glm::normalize(glm::vec3(.5f, .3f, 1.f)), w, h);
}

void Scene::EndRender() {
    prims_.FlushLines();
    grid_.Draw(cam_.View(), cam_.Projection(
        static_cast<float>(fbo_.Width()) / std::max(1, fbo_.Height())),
        cam_.Position(), cam_.Distance());
    fbo_.Resolve();
}

} // namespace KiloScope::Render
