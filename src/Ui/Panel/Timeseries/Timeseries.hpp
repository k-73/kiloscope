#pragma once
#include "../Panel.hpp"

namespace KiloScope::UI {

class Timeseries : public Panel {
public:
    explicit Timeseries(std::shared_ptr<Data::DataStore> s) : Panel("Time Series", std::move(s)) {}
    void Draw() override;

private:
    static constexpr size_t MaxDisplay = 8192;
    std::vector<Data::Sample> buf_ = std::vector<Data::Sample>(MaxDisplay);
    float historySec_ = 10.f;
};

} // namespace KiloScope::UI
