#pragma once
#include <nlohmann/json.hpp>
#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <cstdint>

namespace Kilo {

using json = nlohmann::json;

// ── panel flags ─────────────────────────────────────────────────

enum class PanelFlags : uint8_t {
    None       = 0,
    NoSettings = 1 << 0,
    Singleton  = 1 << 1,
};

inline PanelFlags operator|(PanelFlags a, PanelFlags b) {
    return static_cast<PanelFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline bool operator&(PanelFlags a, PanelFlags b) {
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

// ── panel base ──────────────────────────────────────────────────

class PanelManager;

class Panel {
public:
    Panel(std::string_view typeId, std::string title,
          PanelFlags flags = PanelFlags::None);
    virtual ~Panel();

    // Lifecycle (worker thread calls OnLoop; main thread calls OnDraw)
    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnLoop()   {}
    virtual void OnDraw()   = 0;

    // Persistence
    virtual json SaveSettings() const { return {}; }
    virtual void LoadSettings(const json&) {}

    // Identity
    const std::string& TypeId() const { return typeId_; }
    const std::string& Id()     const { return id_; }
    const std::string& Title()  const { return title_; }
    PanelFlags         Flags()  const { return flags_; }

    // Visibility (atomic — safe to read/write from any thread)
    bool IsVisible()          const { return visible_; }
    void SetVisible(bool v)         { visible_ = v; }

    void Draw();

private:
    friend class PanelManager;

    std::string       typeId_;
    std::string       id_;
    std::string       title_;
    PanelFlags        flags_;
    std::atomic<bool> visible_{true};
    std::mutex        mutex_;

    static int NextInstanceId(const std::string& typeId);
};

} // namespace Kilo
