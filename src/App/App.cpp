#include "App.hpp"
#include "Core/Log.hpp"
#include "Net/Packet.hpp"
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

namespace KiloScope::App {

App::App()
    : store_(std::make_shared<Data::DataStore>())
{
    SetBus(dispatcher_);
    config_.LoadFromFile("kiloscope.json");
    Log::SetLevel(config_.logLevel);

    ui_ = std::make_unique<UI::UiContext>(config_);
    panels_ = std::make_unique<UI::PanelManager>(store_);

    panels_->LoadFromFile(config_.panelConfigPath);
    if (panels_->Empty())
        CreateDefaultPanels();

    receiver_ = std::make_unique<Net::UdpReceiver>(config_.udpPort,
        [this](std::span<const uint8_t> raw) {
            if (auto pkt = Net::ParsePacket(raw)) store_->Ingest(*pkt);
        });

    Log::App().info("KiloScope initialized");
}

App::~App() {
    ui_->SaveWindowState(config_);
    panels_->SaveToFile(config_.panelConfigPath);
    config_.SaveToFile("kiloscope.json");
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
        dispatcher_.update();
        ui_->BeginFrame();
        panels_->DrawMenuBar();
        panels_->Update();
        panels_->Draw();
        ui_->EndFrame();
        glfwSwapBuffers(w);
    }
}

void App::CreateDefaultPanels() {
    panels_->Add("Controls");
    panels_->Add("Timeseries");
    panels_->Add("Scatter");
    panels_->Add("Histogram");
    panels_->Add("Viewport3d");
    panels_->Add("Diagnostics");
    Log::App().info("Created default panel layout");
}

} // namespace KiloScope::App
