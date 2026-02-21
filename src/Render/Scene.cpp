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

    view_   = cam_.View();
    proj_   = cam_.Projection(aspect);
    camPos_ = cam_.Position();

    prims_.Begin(view_, proj_, camPos_, glm::normalize(glm::vec3(.5f, .3f, 1.f)), w, h);
}

void Scene::EndRender() {
    prims_.FlushLines();
    grid_.Draw(view_, proj_, camPos_, cam_.Distance());
    fbo_.Resolve();
}

} // namespace KiloScope::Render
