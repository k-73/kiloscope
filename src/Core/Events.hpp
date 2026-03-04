#pragma once
#include <entt/signal/dispatcher.hpp>
#include <string>

namespace KiloScope {

struct SettingsChanged {
    std::string panelId;
};

entt::dispatcher& Bus();
void SetBus(entt::dispatcher& d);

} // namespace KiloScope
