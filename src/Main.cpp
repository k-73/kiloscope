#include "App/App.hpp"
#include <iostream>

int main() {
    try { KiloScope::App::App{}.Run(); }
    catch (const std::exception& e) { std::cerr << "Fatal: " << e.what() << '\n'; return 1; }
}
