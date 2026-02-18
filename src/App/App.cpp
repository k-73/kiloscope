#include "App.hpp"
#include "Core/Log.hpp"
#include "Net/Packet.hpp"
#include <GLFW/glfw3.h>

namespace KiloScope::App {

App::App()
    : store_(std::make_shared<Data::DataStore>())
    , ui_(std::make_unique<UI::UiContext>())
    , panels_(std::make_unique<UI::PanelManager>(store_))
{
    SetBus(dispatcher_);
    config_.LoadFromFile("kiloscope.json");

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
    panels_->SaveToFile(config_.panelConfigPath);
    config_.SaveToFile("kiloscope.json");
}

void App::Run() {
    auto* w = ui_->Window();
    while (!glfwWindowShouldClose(w)) {
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
    Log::App().info("Created default panel layout");
}

} // namespace KiloScope::App
