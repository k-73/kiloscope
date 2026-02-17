#pragma once
#include "../Panel.hpp"

namespace ks::ui {

class Histogram : public Panel {
public:
    explicit Histogram(std::shared_ptr<data::DataStore> s) : Panel("Histogram", std::move(s)) {}
    void Draw() override;

private:
    static constexpr size_t MaxDisplay = 8192;
    std::vector<data::Sample> buf_ = std::vector<data::Sample>(MaxDisplay);
    int bins_ = 64;
};

} // namespace ks::ui
