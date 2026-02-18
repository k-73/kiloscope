#include "PanelRegistry.hpp"
#include "Core/Log.hpp"

namespace KiloScope::UI {

PanelRegistry& PanelRegistry::Instance() {
    static PanelRegistry inst;
    return inst;
}

void PanelRegistry::Register(PanelEntry entry) {
    entries_.push_back(std::move(entry));
}

std::unique_ptr<Panel> PanelRegistry::Create(std::string_view typeId,
                                              std::shared_ptr<Data::DataStore> store) const {
    for (auto& e : entries_) {
        if (e.typeId == typeId) return e.factory(std::move(store));
    }
    Log::UI().error("Unknown panel type: {}", typeId);
    return nullptr;
}

} // namespace KiloScope::UI
