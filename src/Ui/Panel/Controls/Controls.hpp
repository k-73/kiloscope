#pragma once
#include "../Panel.hpp"

namespace KiloScope::UI {

class Controls : public Panel {
public:
    explicit Controls(std::shared_ptr<Data::DataStore> s) : Panel("Controls", std::move(s)) {}
    void Draw() override;
};

} // namespace KiloScope::UI
