#pragma once
#include "../Panel.hpp"

namespace ks::ui {

class Scatter : public Panel {
public:
    explicit Scatter(std::shared_ptr<data::DataStore> s) : Panel("Scatter Plot", std::move(s)) {}
    void Draw() override;

private:
    static constexpr size_t MaxDisplay = 4096;
    std::vector<data::Sample> bufX_ = std::vector<data::Sample>(MaxDisplay);
    std::vector<data::Sample> bufY_ = std::vector<data::Sample>(MaxDisplay);
    int chX_ = 0, chY_ = 1;
};

} // namespace ks::ui
