#pragma once
#include "../Panel.hpp"
#include <vector>

namespace KiloScope::UI {

class Timeseries : public Panel {
public:
    explicit Timeseries(std::shared_ptr<Data::DataStore> s)
        : Panel("Timeseries", "Time Series", std::move(s)) {}

    void OnUpdate() override;
    void OnDraw() override;
    json SaveSettings() const override;
    void LoadSettings(const json& j) override;

private:
    static constexpr size_t MaxDisplay = 8192;
    std::vector<Data::Sample> buf_{MaxDisplay};
    std::vector<double> xs_{MaxDisplay};
    std::vector<double> ys_{MaxDisplay};
    float historySec_ = 10.f;

    struct ChannelPlotData {
        size_t count = 0;
        std::string name;
    };
    std::vector<ChannelPlotData> plotData_;
    std::vector<size_t> offsets_;  // per-channel offset into xs_/ys_
};

} // namespace KiloScope::UI
