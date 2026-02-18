#pragma once
#include "Data/DataStore.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <cstdint>

namespace KiloScope::UI {

using json = nlohmann::json;

enum class PanelFlags : uint8_t {
    None      = 0,
    NoSettings = 1 << 0,
    Singleton  = 1 << 1,
    NeedsScene = 1 << 2,
};

inline PanelFlags operator|(PanelFlags a, PanelFlags b) {
    return static_cast<PanelFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline bool operator&(PanelFlags a, PanelFlags b) {
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

class Panel {
public:
    Panel(std::string_view typeId, std::string title,
          std::shared_ptr<Data::DataStore> store, PanelFlags flags = PanelFlags::None);
    virtual ~Panel() = default;

    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnUpdate() {}
    virtual void OnDraw() = 0;

    virtual json SaveSettings() const { return {}; }
    virtual void LoadSettings(const json&) {}

    const std::string& TypeId() const { return typeId_; }
    const std::string& Id()     const { return id_; }
    const std::string& Title()  const { return title_; }
    PanelFlags Flags()          const { return flags_; }
    bool IsVisible()            const { return visible_; }
    void SetVisible(bool v)           { visible_ = v; }

    void Draw();  // wraps ImGui::Begin/End + calls OnDraw()

protected:
    std::string typeId_;
    std::string id_;       // "TypeId##N" — unique ImGui ID
    std::string title_;    // display name for menus
    std::shared_ptr<Data::DataStore> store_;
    PanelFlags flags_;
    bool visible_ = true;

private:
    static int NextInstanceId(const std::string& typeId);
};

} // namespace KiloScope::UI
