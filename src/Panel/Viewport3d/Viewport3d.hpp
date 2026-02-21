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

    struct Snapshot {
        std::vector<glm::vec3> path;
        glm::vec3 endpoint{0.f};
        bool valid = false;
    };

    std::vector<Data::Sample> sampleBufs_[3];
    Snapshot pending_;   // written by OnData
    Snapshot active_;    // read by OnRender
    bool newData_ = false;
};

} // namespace KiloScope
