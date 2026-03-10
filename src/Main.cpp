#include "Core/Log.hpp"
#include "App/App.hpp"

int main() {
    Kilo::Log::Init();
    try { Kilo::App::App{}.Run(); }
    catch (const std::exception& e) { Kilo::Log::App().critical("Fatal: {}", e.what()); return 1; }
}
