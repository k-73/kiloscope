#pragma once
#include "Core/Config.hpp"

struct GLFWwindow;

namespace Kilo::UI {

class UiContext {
public:
    explicit UiContext(const AppConfig& config);
    ~UiContext();
    UiContext(const UiContext&) = delete;
    UiContext& operator=(const UiContext&) = delete;

    GLFWwindow* Window() const { return win_; }
    void BeginFrame();
    void EndFrame();
    void SaveWindowState(AppConfig& cfg) const;

private:
    GLFWwindow* win_ = nullptr;
    void InitGlfw(const AppConfig& config);
    void InitImGui();
    void ApplyStyle();
};

} // namespace Kilo::UI
