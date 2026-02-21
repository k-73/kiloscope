#pragma once
#include "Core/Panel/Panel.hpp"
#include <vector>

namespace KiloScope {

class Timeseries : public Panel {
public:
    Timeseries()
        : Panel("Timeseries", "Time Series") {}

    void OnData() override;
    void OnDraw() override;
    json SaveSettings() const override;
    void LoadSettings(const json& j) override;

private:
    static constexpr size_t MaxDisplay = 8192;
    std::vector<Data::Sample> buf_{MaxDisplay};
    std::vector<double> xs_, ys_;
    float historySec_ = 10.f;

    struct ChannelPlotData { size_t count = 0; std::string name; };
    std::vector<ChannelPlotData> plotData_;
    std::vector<size_t> offsets_;
};

} // namespace KiloScope
