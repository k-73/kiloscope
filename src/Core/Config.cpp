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
        if (j.contains("windowWidth"))     windowWidth     = j["windowWidth"].get<int>();
        if (j.contains("windowHeight"))    windowHeight    = j["windowHeight"].get<int>();
        if (j.contains("windowPosX"))      windowPosX      = j["windowPosX"].get<int>();
        if (j.contains("windowPosY"))      windowPosY      = j["windowPosY"].get<int>();
        if (j.contains("maximized"))       maximized       = j["maximized"].get<bool>();
        if (j.contains("decorated"))       decorated       = j["decorated"].get<bool>();
        if (j.contains("vsync"))           vsync           = j["vsync"].get<bool>();
        if (j.contains("logLevel"))        logLevel        = j["logLevel"].get<std::string>();
        Log::App().info("Config loaded from {}", path);
    } catch (const std::exception& e) {
        Log::App().error("Failed to load config from {}: {}", path, e.what());
    }
}

void AppConfig::SaveToFile(const std::string& path) const {
    nlohmann::json j;
    j["udpPort"]         = udpPort;
    j["panelConfigPath"] = panelConfigPath;
    j["windowWidth"]     = windowWidth;
    j["windowHeight"]    = windowHeight;
    j["windowPosX"]      = windowPosX;
    j["windowPosY"]      = windowPosY;
    j["maximized"]       = maximized;
    j["decorated"]       = decorated;
    j["vsync"]           = vsync;
    j["logLevel"]        = logLevel;
    std::ofstream f(path);
    if (f) {
        f << j.dump(2);
        Log::App().info("Config saved to {}", path);
    } else {
        Log::App().error("Failed to save config to {}", path);
    }
}

} // namespace KiloScope
