#include "Core/Log.hpp"
#include "App/App.hpp"

int main() {
    KiloScope::Log::Init();
    try { KiloScope::App::App{}.Run(); }
    catch (const std::exception& e) { KiloScope::Log::App().critical("Fatal: {}", e.what()); return 1; }
}
