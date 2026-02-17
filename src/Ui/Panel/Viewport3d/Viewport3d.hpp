#pragma once
#include "../Panel.hpp"
#include "Render/Scene.hpp"
#include <memory>

namespace ks::ui {

class Viewport3d : public Panel {
public:
    explicit Viewport3d(std::shared_ptr<data::DataStore> s)
        : Panel("3D Viewport", std::move(s)), scene_(std::make_unique<render::Scene>()) {}
    void Draw() override;

private:
    std::unique_ptr<render::Scene> scene_;
    bool init_ = false;
};

} // namespace ks::ui
