#pragma once

struct GLFWwindow;

namespace KiloScope::UI {

class UiContext {
public:
    UiContext();
    ~UiContext();
    UiContext(const UiContext&) = delete;
    UiContext& operator=(const UiContext&) = delete;

    GLFWwindow* Window() const { return win_; }
    void BeginFrame();
    void EndFrame();

private:
    GLFWwindow* win_ = nullptr;
    void InitGlfw();
    void InitImGui();
    void ApplyStyle();
};

} // namespace KiloScope::UI
