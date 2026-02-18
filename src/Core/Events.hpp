#pragma once
#include <entt/signal/dispatcher.hpp>
#include <cstdint>
#include <string>

namespace KiloScope {

// ── Event types ─────────────────────────────────────────────────────
struct DataCleared {};

struct ChannelAdded {
    uint16_t channelId;
};

struct SettingsChanged {
    std::string panelId;
};

// ── Global event bus ────────────────────────────────────────────────
entt::dispatcher& Bus();
void SetBus(entt::dispatcher& d);

} // namespace KiloScope
