#pragma once
#include "../Panel.hpp"

namespace KiloScope::UI {

class Histogram : public Panel {
public:
    explicit Histogram(std::shared_ptr<Data::DataStore> s) : Panel("Histogram", std::move(s)) {}
    void Draw() override;

private:
    static constexpr size_t MaxDisplay = 8192;
    std::vector<Data::Sample> buf_ = std::vector<Data::Sample>(MaxDisplay);
    int bins_ = 64;
};

} // namespace KiloScope::UI
