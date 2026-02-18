#include "Events.hpp"
#include <cassert>

namespace KiloScope {

static entt::dispatcher* sBus = nullptr;

entt::dispatcher& Bus() {
    assert(sBus && "Bus not initialized — call SetBus() first");
    return *sBus;
}

void SetBus(entt::dispatcher& d) { sBus = &d; }

} // namespace KiloScope
