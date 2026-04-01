#pragma once
#include "Panel.hpp"
#include "PanelRegistry.hpp"
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Kilo {

// Lightweight snapshot of per-panel timing, updated each frame by PanelManager.
// Diagnostics (or any panel) reads this without needing PanelManager access.
// Peaks live only here — ResetPeaks zeroes them cleanly.
struct PanelTimingEntry {
    const char* title   = "";
    float drawUs        = 0.f;
    float loopUs        = 0.f;
    float drawPeak      = 0.f;   // peak since last reset
    float loopPeak      = 0.f;
    float mutexWaitUs   = 0.f;
    bool  visible       = false;
};

struct PanelTimingSnapshot {
    std::vector<PanelTimingEntry> panels;
    float workerCycleUs  = 0.f;  // EMA of worker cycle (µs)
    float workerPeakUs   = 0.f;  // peak worker cycle since reset
    int   workerOverruns = 0;    // ticks where cycle > 1ms since reset

    static PanelTimingSnapshot& Get() { static PanelTimingSnapshot s; return s; }

    void ResetPeaks() {
        for (auto& e : panels) { e.drawPeak = e.loopPeak = 0.f; }
        workerPeakUs = 0.f;
        workerOverruns = 0;
    }
};

class PanelManager {
public:
    PanelManager();
    ~PanelManager();

    void   Start();
    Panel* Add(std::string_view typeId);
    void   Remove(std::string_view id);

    void Draw();
    void DrawMenuBar();
    bool Empty() const;

    void SaveToFile(const std::string& path) const;
    void LoadFromFile(const std::string& path);

private:
    void WorkerLoop(std::stop_token st);

    // panels_ guarded by panelsMutex_: shared_lock for reads, unique_lock for writes.
    // Individual panel state guarded by panel->mutex_.
    std::vector<std::unique_ptr<Panel>> panels_;
    mutable std::shared_mutex           panelsMutex_;
    std::jthread                        worker_;
};

} // namespace Kilo
