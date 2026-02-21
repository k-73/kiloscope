#pragma once
#include "Core/Panel/Panel.hpp"
#include "Render/Scene.hpp"
#include <memory>

namespace KiloScope {

class Viewport3d : public Panel {
public:
    Viewport3d()
        : Panel("Viewport3d", "3D Viewport",
                PanelFlags::Singleton | PanelFlags::NeedsScene)
        , scene_(std::make_unique<Render::Scene>()) {}

    void OnDraw() override;

private:
    std::unique_ptr<Render::Scene> scene_;
    bool init_ = false;
};

} // namespace KiloScope
