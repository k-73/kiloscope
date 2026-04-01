#include "Diagnostics.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Core/Panel/PanelManager.hpp"
#include "Render/Draw.hpp"
#include <imgui.h>
#include <implot.h>
#include <GL/glew.h>
#include <algorithm>

namespace Kilo {

static const char* sGpuRenderer = nullptr;
static const char* sGlVersion   = nullptr;

void Diagnostics::OnDraw() {
    auto& io = ImGui::GetIO();
    float fps = io.Framerate;
    float ft  = 1000.f / fps;

    // ── sampling ────────────────────────────────────────────────
    int skip = std::max(1, static_cast<int>(fps * 0.032f));
    if (++sampleSkip_ >= skip) {
        sampleSkip_ = 0;
        fpsHistory_[histIdx_] = fps;
        histIdx_ = (histIdx_ + 1) % kHistory;
    }

    float sum = 0.f, lo = fps, hi = fps;
    int n = 0;
    for (int i = 0; i < kHistory; ++i) {
        float v = fpsHistory_[i];
        if (v > 0.f) { sum += v; lo = std::min(lo, v); hi = std::max(hi, v); ++n; }
    }
    float avg = n > 0 ? sum / float(n) : fps;

    // ── Performance ─────────────────────────────────────────────
    ImGui::SeparatorText("Performance");
    ImGui::Text("%.0f FPS (%.2f ms)   avg %.0f   min %.0f", fps, ft, avg, lo);

    float margin = std::max(1.f, (hi - lo) * 0.15f);
    ImGui::PlotLines("##fps", fpsHistory_, kHistory, histIdx_, nullptr,
                     lo - margin, hi + margin, {-1, 60});

    // ── Panel Timing ────────────────────────────────────────────
    auto& snap = PanelTimingSnapshot::Get();
    ImGui::SeparatorText("Panel Timing (\xc2\xb5s)");

    if (ImGui::BeginTable("##timing", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Panel");
        ImGui::TableSetupColumn("Draw");
        ImGui::TableSetupColumn("Peak##d");
        ImGui::TableSetupColumn("Loop");
        ImGui::TableSetupColumn("Peak##l");
        ImGui::TableSetupColumn("Mutex");
        ImGui::TableHeadersRow();

        for (auto& e : snap.panels) {
            ImGui::TableNextRow();

            bool dim = !e.visible;
            if (dim) ImGui::PushStyleColor(ImGuiCol_Text, {.5f,.5f,.5f,1});

            ImGui::TableNextColumn(); ImGui::TextUnformatted(e.title);

            ImGui::TableNextColumn(); ImGui::Text("%.0f", e.drawUs);
            ImGui::TableNextColumn();
            bool hotDraw = e.drawPeak > 2000.f;
            if (hotDraw) ImGui::PushStyleColor(ImGuiCol_Text, {1,.3f,.3f,1});
            ImGui::Text("%.0f", e.drawPeak);
            if (hotDraw) ImGui::PopStyleColor();

            ImGui::TableNextColumn(); ImGui::Text("%.0f", e.loopUs);
            ImGui::TableNextColumn();
            bool hotLoop = e.loopPeak > 500.f;
            if (hotLoop) ImGui::PushStyleColor(ImGuiCol_Text, {1,.6f,.2f,1});
            ImGui::Text("%.0f", e.loopPeak);
            if (hotLoop) ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            bool hotMutex = e.mutexWaitUs > 100.f;
            if (hotMutex) ImGui::PushStyleColor(ImGuiCol_Text, {1,.5f,.2f,1});
            ImGui::Text("%.0f", e.mutexWaitUs);
            if (hotMutex) ImGui::PopStyleColor();

            if (dim) ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }

    if (ImGui::SmallButton("Reset Peaks")) snap.ResetPeaks();

    // ── Worker Thread ───────────────────────────────────────────
    ImGui::SeparatorText("Worker Thread (1kHz)");
    ImGui::Text("Cycle: %.0f \xc2\xb5s   Peak: %.0f \xc2\xb5s   Overruns: %d",
        snap.workerCycleUs, snap.workerPeakUs, snap.workerOverruns);
    
    // ── Render ──────────────────────────────────────────────────
    ImGui::SeparatorText("Render");
    auto& s = Render::GetStats();
    ImGui::Text("Draw Calls: %d (+%d pick)", s.drawCalls, s.pickDrawCalls);
    ImGui::Text("Vertices: %d  Lines: %d  Points: %d", s.vertices, s.lineSegments, s.points);
    ImGui::Text("Lights: %d  Text: %d", s.pointLights, s.textLabels);
    ImGui::Text("Viewport: %dx%d  MSAA: %dx", s.viewportW, s.viewportH, s.msaaSamples);
    if (s.viewportW > 0 && s.msaaSamples > 0) {
        float fboMB = s.viewportW * s.viewportH * s.msaaSamples * 8.f / (1024.f * 1024.f) * 2.f;
        ImGui::Text("FBO VRAM: ~%.1f MB (color+depth)", fboMB);
    }

    // ── GPU ─────────────────────────────────────────────────────
    ImGui::SeparatorText("GPU");
    if (!sGpuRenderer) sGpuRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    if (!sGlVersion)   sGlVersion   = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    ImGui::Text("GPU: %s", sGpuRenderer ? sGpuRenderer : "?");
    ImGui::Text("GL:  %s", sGlVersion ? sGlVersion : "?");
    ImGui::Text("ImGui Vtx: %d  Idx: %d  Windows: %d",
        io.MetricsRenderVertices, io.MetricsRenderIndices, io.MetricsRenderWindows);

    // ── Demos ───────────────────────────────────────────────────
    ImGui::SeparatorText("Demos");
    ImGui::Checkbox("ImGui Demo", &showImGuiDemo_);
    ImGui::SameLine();
    ImGui::Checkbox("ImPlot Demo", &showImPlotDemo_);

    if (showImGuiDemo_)  ImGui::ShowDemoWindow(&showImGuiDemo_);
    if (showImPlotDemo_) ImPlot::ShowDemoWindow(&showImPlotDemo_);
}

static const bool reg_ = RegisterPanel<Diagnostics>("Diagnostics", "Diagnostics");

} // namespace Kilo
