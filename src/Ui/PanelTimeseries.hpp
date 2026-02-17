#pragma once
#include "Panel.hpp"

namespace ks::ui {

class PanelTimeseries : public Panel {
public:
    explicit PanelTimeseries(std::shared_ptr<data::DataStore> s) : Panel("Time Series", std::move(s)) {}
    void Draw() override;

private:
    static constexpr size_t MaxDisplay = 8192;
    std::vector<data::Sample> buf_ = std::vector<data::Sample>(MaxDisplay);
    float historySec_ = 10.f;
};

} // namespace ks::ui
