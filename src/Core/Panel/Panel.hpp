#pragma once
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <cstdint>

namespace Kilo::Render { class Scene; }

namespace Kilo {

using json = nlohmann::json;

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

struct ViewportConfig {
    float width  = -1;  // -1 = fill available
    float height = -1;
};

class PanelManager;

class Panel {
public:
    Panel(std::string_view typeId, std::string title,
          PanelFlags flags = PanelFlags::None);
    virtual ~Panel();

    virtual void OnAttach() {}
    virtual void OnDetach() {}
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

    void Draw();

protected:
    void RenderBegin(const char* name, const ViewportConfig& cfg = {});
    void RenderEnd();

private:
    friend class PanelManager;

    std::string typeId_;
    std::string id_;
    std::string title_;
    PanelFlags flags_;
    bool visible_ = true;

    std::string shaderDir_;
    std::unordered_map<std::string, std::unique_ptr<Render::Scene>> scenes_;
    std::mutex mutex_;

    struct SceneFrame {
        Render::Scene* scene{};
        float cursorX{}, cursorY{}, w{}, h{};
    } sf_;

    static int NextInstanceId(const std::string& typeId);
};

} // namespace Kilo
