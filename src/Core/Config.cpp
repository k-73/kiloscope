#include "Config.hpp"
#include "Log.hpp"
#include <fstream>

namespace KiloScope {

void AppConfig::LoadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        Log::App().warn("No config file at {}, using defaults", path);
        return;
    }
    try {
        auto j = nlohmann::json::parse(f);
        if (j.contains("udpPort"))         udpPort         = j["udpPort"].get<uint16_t>();
        if (j.contains("panelConfigPath")) panelConfigPath = j["panelConfigPath"].get<std::string>();
        Log::App().info("Config loaded from {}", path);
    } catch (const std::exception& e) {
        Log::App().error("Failed to load config from {}: {}", path, e.what());
    }
}

void AppConfig::SaveToFile(const std::string& path) const {
    nlohmann::json j;
    j["udpPort"]         = udpPort;
    j["panelConfigPath"] = panelConfigPath;
    std::ofstream f(path);
    if (f) {
        f << j.dump(2);
        Log::App().info("Config saved to {}", path);
    } else {
        Log::App().error("Failed to save config to {}", path);
    }
}

} // namespace KiloScope
