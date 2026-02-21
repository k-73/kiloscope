#pragma once
#include "Fbo.hpp"
#include "Camera.hpp"
#include "Grid.hpp"
#include "Primitives.hpp"
#include <string>

namespace KiloScope::Render {

class Scene {
public:
    void Init(const std::string& shaderDir);
    void Resize(int w, int h) { fbo_.Resize(w, h, 8); }

    void BeginRender();
    void EndRender();
    GLuint Texture() const { return fbo_.Texture(); }

    Camera&     GetCamera() { return cam_; }
    Primitives& Prims()     { return prims_; }

private:
    Fbo fbo_;
    Camera cam_;
    Grid grid_;
    Primitives prims_;
};

} // namespace KiloScope::Render
