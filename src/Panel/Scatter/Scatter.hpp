#pragma once
#include "Core/Panel/Panel.hpp"
#include <vector>

namespace KiloScope {

class Scatter : public Panel {
public:
    Scatter()
        : Panel("Scatter", "Scatter Plot") {}

    void OnData() override;
    void OnDraw() override;
    json SaveSettings() const override;
    void LoadSettings(const json& j) override;

private:
    static constexpr size_t MaxDisplay = 4096;
    std::vector<Data::Sample> bufX_{MaxDisplay}, bufY_{MaxDisplay};
    std::vector<double> xs_, ys_;
    int chX_ = 0, chY_ = 1;
    size_t plotCount_ = 0;
};

} // namespace KiloScope
