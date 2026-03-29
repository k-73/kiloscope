#include "UiContext.hpp"
#include "Core/Log.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <imgui_freetype.h>
#include "Ui/IconsFontAwesome7.h"
#include <stdexcept>

namespace Kilo::UI {

UiContext::UiContext(const AppConfig& config)  { InitGlfw(config); InitImGui(); ApplyStyle(); }

UiContext::~UiContext() {
    ImPlot::DestroyContext();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (win_) glfwDestroyWindow(win_);
    glfwTerminate();
}

void UiContext::InitGlfw(const AppConfig& config) {
    glfwSetErrorCallback([](int c, const char* d) { Log::Render().error("GLFW {}: {}", c, d); });
    if (!glfwInit()) throw std::runtime_error("GLFW init failed");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, config.maximized ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);
    win_ = glfwCreateWindow(config.windowWidth, config.windowHeight, "Kilo", nullptr, nullptr);
    if (!win_) throw std::runtime_error("Window creation failed");
    if (!config.maximized && config.windowPosX != -1)
        glfwSetWindowPos(win_, config.windowPosX, config.windowPosY);
    glfwMakeContextCurrent(win_);
    glfwSwapInterval(config.vsync ? 1 : 0);
    if (glewInit() != GLEW_OK) throw std::runtime_error("GLEW init failed");
}

void UiContext::InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
    io.Fonts->AddFontDefault();

    // Merge icon fonts (Font Awesome 7 + any additional icon fonts from assets/fonts/)
    ImFontConfig iconCfg;
    iconCfg.MergeMode = true;
    iconCfg.PixelSnapH = true;
    iconCfg.GlyphMinAdvanceX = 13.f;
    static const ImWchar faRange[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    auto faPath = std::string(ASSETS_DIR) + "/fonts/fa-solid-900.otf";
    io.Fonts->AddFontFromFileTTF(faPath.c_str(), 13.f, &iconCfg, faRange);

    ImGui_ImplGlfw_InitForOpenGL(win_, true);
    ImGui_ImplOpenGL3_Init("#version 450");
}

void UiContext::ApplyStyle() {
    ImGui::StyleColorsDark();
    auto& s = ImGui::GetStyle();
    s.WindowRounding = 4; s.FrameRounding = 3; s.GrabRounding = 3;
    s.TabRounding = 3; s.ScrollbarRounding = 3;
    s.FramePadding = {6, 4}; s.ItemSpacing = {8, 5};
    s.WindowBorderSize = 1; s.FrameBorderSize = 0;
    auto& c = s.Colors;
    c[ImGuiCol_WindowBg]      = {.10f, .10f, .12f, 1};
    c[ImGuiCol_TitleBg]       = {.08f, .08f, .10f, 1};
    c[ImGuiCol_TitleBgActive] = {.14f, .14f, .18f, 1};
    c[ImGuiCol_Tab]                      = {.14f, .14f, .18f, 1};
    c[ImGuiCol_TabHovered]               = {.22f, .22f, .28f, 1};
    c[ImGuiCol_TabSelected]              = {.24f, .24f, .32f, 1};
    c[ImGuiCol_TabSelectedOverline]      = {.36f, .36f, .48f, 1};
    c[ImGuiCol_TabDimmed]                = {.10f, .10f, .13f, 1};
    c[ImGuiCol_TabDimmedSelected]        = {.18f, .18f, .24f, 1};
    c[ImGuiCol_TabDimmedSelectedOverline]= {.26f, .26f, .36f, 1};
    c[ImGuiCol_FrameBg]       = {.14f, .14f, .18f, 1};
    c[ImGuiCol_Header]        = {.20f, .20f, .26f, 1};
    c[ImGuiCol_HeaderHovered] = {.26f, .26f, .34f, 1};
    c[ImGuiCol_HeaderActive]  = {.30f, .30f, .40f, 1};
    ImPlot::StyleColorsDark();
    ImPlot::GetStyle().PlotDefaultSize = {400, 300};
}

void UiContext::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    auto* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::Begin("##Dock", nullptr,
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(3);
    ImGui::DockSpace(ImGui::GetID("Main"), {0, 0}, ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}

void UiContext::EndFrame() {
    ImGui::Render();
    int w, h; glfwGetFramebufferSize(win_, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(.08f, .08f, .10f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UiContext::SaveWindowState(AppConfig& cfg) const {
    cfg.maximized = glfwGetWindowAttrib(win_, GLFW_MAXIMIZED) != 0;
    if (!cfg.maximized) {
        glfwGetWindowPos(win_, &cfg.windowPosX, &cfg.windowPosY);
        glfwGetWindowSize(win_, &cfg.windowWidth, &cfg.windowHeight);
    }
}

} // namespace Kilo::UI
