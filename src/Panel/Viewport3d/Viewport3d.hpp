#pragma once
#include "Core/Panel/Panel.hpp"
#include "Data/Channel.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace KiloScope {

class Viewport3d : public Panel {
public:
    Viewport3d()
        : Panel("Viewport3d", "3D Viewport",
                PanelFlags::Singleton | PanelFlags::NeedsScene) {}

    void OnData(Data::DataStore& store) override;
    void OnDraw() override;

protected:
    void OnRender(Render::Scene& scene) override;

private:
    static constexpr size_t MaxPts = 4096;

    struct PointCache {
        std::vector<glm::vec3> path;
        glm::vec3 endpoint{0.f};
        bool empty = true;
    };

    PointCache cache_;
};

} // namespace KiloScope
