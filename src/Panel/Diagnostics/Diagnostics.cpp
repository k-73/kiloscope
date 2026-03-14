#include "Diagnostics.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include <imgui.h>
#include <implot.h>
#include <GL/glew.h>

namespace Kilo {

static const char* sGpuRenderer = nullptr;
static const char* sGlVersion   = nullptr;

void Diagnostics::OnDraw() {
    auto& io = ImGui::GetIO();

    frameTimes_[frameIdx_] = 1000.f / io.Framerate;
    frameIdx_ = (frameIdx_ + 1) % 128;

    ImGui::SeparatorText("Performance");
    ImGui::Text("%.1f FPS  (%.2f ms)", io.Framerate, 1000.f / io.Framerate);
    ImGui::PlotLines("##ft", frameTimes_, 128, frameIdx_, nullptr, 0.f, 33.f, {-1, 40});

    ImGui::SeparatorText("Render");
    auto& s = Render::GetStats();
    ImGui::Text("Draw Calls: %d (+%d pick +%d shadow)", s.drawCalls, s.pickDrawCalls, s.shadowDrawCalls);
    ImGui::Text("Vertices: %d  Lines: %d  Points: %d", s.vertices, s.lineSegments, s.points);
    ImGui::Text("Lights: %d  Text: %d", s.pointLights, s.textLabels);
    ImGui::Text("Viewport: %dx%d  MSAA: %dx", s.viewportW, s.viewportH, s.msaaSamples);
    if (s.viewportW > 0 && s.msaaSamples > 0) {
        float fboMB = s.viewportW * s.viewportH * s.msaaSamples * 8.f / (1024.f * 1024.f) * 2.f;
        ImGui::Text("FBO VRAM: ~%.1f MB (color+depth)", fboMB);
    }

    ImGui::SeparatorText("GPU");
    if (!sGpuRenderer) sGpuRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    if (!sGlVersion)   sGlVersion   = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    ImGui::Text("GPU: %s", sGpuRenderer ? sGpuRenderer : "?");
    ImGui::Text("GL:  %s", sGlVersion ? sGlVersion : "?");
    ImGui::Text("ImGui Vtx: %d  Idx: %d  Windows: %d",
        io.MetricsRenderVertices, io.MetricsRenderIndices, io.MetricsRenderWindows);

    ImGui::SeparatorText("Demos");
    ImGui::Checkbox("ImGui Demo", &showImGuiDemo_);
    ImGui::SameLine();
    ImGui::Checkbox("ImPlot Demo", &showImPlotDemo_);

    if (showImGuiDemo_)  ImGui::ShowDemoWindow(&showImGuiDemo_);
    if (showImPlotDemo_) ImPlot::ShowDemoWindow(&showImPlotDemo_);
}

static const bool reg_ = RegisterPanel<Diagnostics>("Diagnostics", "Diagnostics");

} // namespace Kilo
