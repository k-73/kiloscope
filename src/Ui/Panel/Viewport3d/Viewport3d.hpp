#pragma once
#include "../Panel.hpp"
#include "Render/Scene.hpp"
#include <memory>

namespace KiloScope::UI {

class Viewport3d : public Panel {
public:
    explicit Viewport3d(std::shared_ptr<Data::DataStore> s)
        : Panel("3D Viewport", std::move(s)), scene_(std::make_unique<Render::Scene>()) {}
    void Draw() override;

private:
    std::unique_ptr<Render::Scene> scene_;
    bool init_ = false;
};

} // namespace KiloScope::UI
