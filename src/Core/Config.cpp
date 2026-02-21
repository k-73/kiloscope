#include "Config.hpp"
#include "Log.hpp"
#include <fstream>

namespace KiloScope {

void to_json(nlohmann::json& j, const AppConfig& c) {
    j = {{"udpPort", c.udpPort}, {"panelConfigPath", c.panelConfigPath},
         {"windowWidth", c.windowWidth}, {"windowHeight", c.windowHeight},
         {"windowPosX", c.windowPosX}, {"windowPosY", c.windowPosY},
         {"maximized", c.maximized}, {"decorated", c.decorated},
         {"vsync", c.vsync}, {"logLevel", c.logLevel}};
}

void from_json(const nlohmann::json& j, AppConfig& c) {
    c.udpPort         = j.value("udpPort", c.udpPort);
    c.panelConfigPath = j.value("panelConfigPath", c.panelConfigPath);
    c.windowWidth     = j.value("windowWidth", c.windowWidth);
    c.windowHeight    = j.value("windowHeight", c.windowHeight);
    c.windowPosX      = j.value("windowPosX", c.windowPosX);
    c.windowPosY      = j.value("windowPosY", c.windowPosY);
    c.maximized       = j.value("maximized", c.maximized);
    c.decorated       = j.value("decorated", c.decorated);
    c.vsync           = j.value("vsync", c.vsync);
    c.logLevel        = j.value("logLevel", c.logLevel);
}

void AppConfig::LoadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) { Log::App().warn("No config at {}, using defaults", path); return; }
    try {
        nlohmann::json::parse(f).get_to(*this);
        Log::App().info("Config loaded from {}", path);
    } catch (const std::exception& e) {
        Log::App().error("Failed to load config: {}", e.what());
    }
}

void AppConfig::SaveToFile(const std::string& path) const {
    std::ofstream f(path);
    if (!f) { Log::App().error("Failed to save config to {}", path); return; }
    f << nlohmann::json(*this).dump(2);
    Log::App().info("Config saved to {}", path);
}

} // namespace KiloScope
