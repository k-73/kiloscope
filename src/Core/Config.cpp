#include "Config.hpp"
#include "Log.hpp"
#include <glaze/glaze.hpp>
#include <fstream>

template<>
struct glz::meta<Kilo::AppConfig> {
    using T = Kilo::AppConfig;
    static constexpr auto value = object(
        "panelConfigPath", &T::panelConfigPath,
        "windowWidth",     &T::windowWidth,
        "windowHeight",    &T::windowHeight,
        "windowPosX",      &T::windowPosX,
        "windowPosY",      &T::windowPosY,
        "maximized",       &T::maximized,
        "decorated",       &T::decorated,
        "vsync",           &T::vsync,
        "msaa",            &T::msaa,
        "logLevel",        &T::logLevel
    );
};

namespace Kilo {

void AppConfig::LoadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) { Log::App().warn("No config at {}, using defaults", path); return; }
    std::string content((std::istreambuf_iterator<char>(f)), {});
    auto ec = glz::read_json(*this, content);
    if (ec) {
        Log::App().error("Failed to load config: {}", glz::format_error(ec, content));
    } else {
        Log::App().info("Config loaded from {}", path);
    }
}

void AppConfig::SaveToFile(const std::string& path) const {
    std::ofstream f(path);
    if (!f) { Log::App().error("Failed to save config to {}", path); return; }
    std::string buffer;
    auto ec = glz::write<glz::opts{.prettify = true}>(*this, buffer);
    if (ec) { Log::App().error("Failed to serialize config"); return; }
    f << buffer;
    Log::App().info("Config saved to {}", path);
}

} // namespace Kilo
