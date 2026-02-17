#pragma once
#include "../Panel.hpp"

namespace KiloScope::UI {

class Scatter : public Panel {
public:
    explicit Scatter(std::shared_ptr<Data::DataStore> s) : Panel("Scatter Plot", std::move(s)) {}
    void Draw() override;

private:
    static constexpr size_t MaxDisplay = 4096;
    std::vector<Data::Sample> bufX_ = std::vector<Data::Sample>(MaxDisplay);
    std::vector<Data::Sample> bufY_ = std::vector<Data::Sample>(MaxDisplay);
    int chX_ = 0, chY_ = 1;
};

} // namespace KiloScope::UI
