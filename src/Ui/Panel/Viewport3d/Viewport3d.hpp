#pragma once
#include "../Panel.hpp"
#include "Render/Scene.hpp"
#include <memory>

namespace KiloScope::UI {

class Viewport3d : public Panel {
public:
    explicit Viewport3d(std::shared_ptr<Data::DataStore> s)
        : Panel("Viewport3d", "3D Viewport", std::move(s),
                PanelFlags::Singleton | PanelFlags::NeedsScene)
        , scene_(std::make_unique<Render::Scene>()) {}

    void OnDraw() override;

private:
    std::unique_ptr<Render::Scene> scene_;
    bool init_ = false;
};

} // namespace KiloScope::UI
