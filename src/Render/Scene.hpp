#pragma once
#include "Fbo.hpp"
#include "Camera.hpp"
#include "Grid.hpp"
#include "Primitives.hpp"
#include "GeometryRenderer.hpp"
#include "Data/DataStore.hpp"
#include <memory>
#include <string>

namespace ks::render {

class Scene {
public:
    void Init(const std::string& shaderDir);
    void Resize(int w, int h) { fbo_.Resize(w, h, 8); }
    GLuint Render(std::shared_ptr<data::DataStore> store);
    Camera& GetCamera() { return cam_; }

private:
    Fbo fbo_;
    Camera cam_;
    Grid grid_;
    Primitives prims_;
    GeometryRenderer geom_;
};

} // namespace ks::render
