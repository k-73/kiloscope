#include "App/App.hpp"
#include <iostream>

int main() {
    try { ks::app::App{}.Run(); }
    catch (const std::exception& e) { std::cerr << "Fatal: " << e.what() << '\n'; return 1; }
}
