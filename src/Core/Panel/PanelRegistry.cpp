#include "Core/Panel/PanelRegistry.hpp"
#include "Core/Log.hpp"

namespace KiloScope {

PanelRegistry& PanelRegistry::Instance() {
    static PanelRegistry inst;
    return inst;
}

void PanelRegistry::Register(PanelEntry entry) {
    entries_.push_back(std::move(entry));
}

std::unique_ptr<Panel> PanelRegistry::Create(std::string_view typeId) const {
    for (auto& e : entries_) {
        if (e.typeId == typeId) return e.factory();
    }
    Log::UI().error("Unknown panel type: {}", typeId);
    return nullptr;
}

} // namespace KiloScope
