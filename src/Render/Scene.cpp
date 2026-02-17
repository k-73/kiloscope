#include "Scene.hpp"

namespace ks::render {

void Scene::Init(const std::string& dir) {
    grid_.Init(dir);
    prims_.Init(dir);
    geom_.Init(prims_);
}

GLuint Scene::Render(std::shared_ptr<data::DataStore> store) {
    fbo_.Bind();
    int w = fbo_.Width(), h = fbo_.Height();
    float aspect = (float)w / std::max(1, h);
    auto view = cam_.View();
    auto proj = cam_.Projection(aspect);
    auto pos  = cam_.Position();

    prims_.Begin(view, proj, pos, glm::normalize(glm::vec3(.5f, 1.f, .3f)), w, h);
    prims_.DrawAxes({0, 0, 0}, 1.f);
    geom_.Draw(store);
    prims_.FlushLines();
    grid_.Draw(view, proj, pos, cam_.Distance());

    fbo_.Resolve();
    return fbo_.Texture();
}

} // namespace ks::render
