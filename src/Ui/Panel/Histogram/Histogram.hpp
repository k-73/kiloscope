#pragma once
#include "../Panel.hpp"
#include <vector>

namespace KiloScope::UI {

class Histogram : public Panel {
public:
    explicit Histogram(std::shared_ptr<Data::DataStore> s)
        : Panel("Histogram", "Histogram", std::move(s)) {}

    void OnUpdate() override;
    void OnDraw() override;
    json SaveSettings() const override;
    void LoadSettings(const json& j) override;

private:
    static constexpr size_t MaxDisplay = 8192;
    std::vector<Data::Sample> buf_{MaxDisplay};
    int bins_ = 64;

    struct ChannelHistData {
        size_t count = 0;
        std::string name;
        size_t offset = 0;
    };
    std::vector<ChannelHistData> histData_;
    std::vector<double> vals_;  // preallocated
};

} // namespace KiloScope::UI
