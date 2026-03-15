#include "App.hpp"
#include "Core/Log.hpp"
#include "Render/Draw.hpp"
#include <GLFW/glfw3.h>
#include <atomic>
#include <csignal>

namespace {
std::atomic<bool> g_shutdown{false};
void OnSignal(int sig) {
    if (g_shutdown.exchange(true)) {
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }
}
}

namespace Kilo::App {

App::App() {
    config_.LoadFromFile("kiloscope.json");
    Log::SetLevel(config_.logLevel);

    ui_ = std::make_unique<UI::UiContext>(config_);
    Render::Init(std::string(ASSETS_DIR) + "/shaders");
    panels_ = std::make_unique<PanelManager>();

    panels_->LoadFromFile(config_.panelConfigPath);
    if (panels_->Empty())
        CreateDefaultPanels();
    panels_->Start();

    Log::App().info("Kilo initialized");
}

App::~App() {
    ui_->SaveWindowState(config_);
    panels_->SaveToFile(config_.panelConfigPath);
    config_.SaveToFile("kiloscope.json");
    Render::Shutdown();  // release GPU resources before GL context dies
}

void App::Run() {
    std::signal(SIGINT,  OnSignal);
    std::signal(SIGTERM, OnSignal);

    auto* w = ui_->Window();
    while (!glfwWindowShouldClose(w)) {
        if (g_shutdown.load(std::memory_order_relaxed)) {
            Log::App().info("Signal received, shutting down");
            glfwSetWindowShouldClose(w, GLFW_TRUE);
            break;
        }
        glfwPollEvents();
        ui_->BeginFrame();
        panels_->DrawMenuBar();
        panels_->Draw();
        ui_->EndFrame();
        glfwSwapBuffers(w);
    }
}

void App::CreateDefaultPanels() {
    panels_->Add("Example");
    panels_->Add("Diagnostics");
    Log::App().info("Created default panel layout");
}

} // namespace Kilo::App
