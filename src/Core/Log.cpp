#include "Log.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Kilo {

std::shared_ptr<spdlog::logger> Log::sApp_;
std::shared_ptr<spdlog::logger> Log::sRender_;
std::shared_ptr<spdlog::logger> Log::sUI_;

void Log::Init() {
    spdlog::set_pattern("[%H:%M:%S.%e] [%n] %^[%l]%$ %v");

    sApp_    = spdlog::stdout_color_mt("App");
    sRender_ = spdlog::stdout_color_mt("Render");
    sUI_     = spdlog::stdout_color_mt("UI");

    spdlog::set_level(spdlog::level::trace);
}

void Log::SetLevel(const std::string& level) {
    spdlog::set_level(spdlog::level::from_str(level));
}

} // namespace Kilo
