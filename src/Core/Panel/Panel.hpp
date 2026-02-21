#pragma once
#include "Data/DataStore.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <cstdint>

namespace KiloScope::Render { class Scene; }

namespace KiloScope {

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

class PanelManager;

class Panel {
public:
    Panel(std::string_view typeId, std::string title,
          PanelFlags flags = PanelFlags::None);
    virtual ~Panel();

    void BindStore(std::shared_ptr<Data::DataStore> store) { store_ = std::move(store); }
    void BindScene(std::unique_ptr<Render::Scene> scene);

    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnData(Data::DataStore& store) {}
    virtual void OnLoop() {}
    virtual void OnDraw() = 0;

    virtual json SaveSettings() const { return {}; }
    virtual void LoadSettings(const json&) {}

    const std::string& TypeId() const { return typeId_; }
    const std::string& Id()     const { return id_; }
    const std::string& Title()  const { return title_; }
    PanelFlags Flags()          const { return flags_; }
    bool IsVisible()            const { return visible_; }
    void SetVisible(bool v)           { visible_ = v; }

    void Draw();  // acquires mutex_, wraps ImGui::Begin/End + calls OnDraw()

protected:
    virtual void OnRender(Render::Scene& scene) {}
    void DrawViewport();

    std::shared_ptr<Data::DataStore> store_;

private:
    friend class PanelManager;

    std::string typeId_;
    std::string id_;
    std::string title_;
    PanelFlags flags_;
    bool visible_ = true;

    std::unique_ptr<Render::Scene> scene_;
    std::mutex mutex_;
    std::atomic<bool> dirty_{false};

    void MarkDirty() { dirty_.store(true, std::memory_order_relaxed); }
    static int NextInstanceId(const std::string& typeId);
};

} // namespace KiloScope
