#pragma once
#include "Core/Panel/Panel.hpp"
#include "Data/Channel.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace KiloScope {

class Viewport3d : public Panel {
public:
    Viewport3d();

    void OnData(Data::DataStore& store) override;
    void OnDraw() override;

protected:
    void OnRender(Render::Scene& scene) override;

private:
    static constexpr size_t MaxPts = 4096;

    std::vector<Data::Sample> sampleBufs_[3];
    std::vector<glm::vec3> path_;
    glm::vec3 endpoint_{0.f};
    bool hasData_ = false;
};

} // namespace KiloScope
