#pragma once
// Reusable ECEF trail buffer — records geodetic positions, converts to local NED/ENU per frame.
// Usage:
//   trail.Record(lat, lon, alt);           // call from worker thread (OnLoop)
//   trail.Draw(color, width);              // call inside Begin/End (OnDraw)

#include "Render/Geo.hpp"
#include "Render/Draw.hpp"
#include <glm/glm.hpp>
#include <deque>
#include <vector>

namespace Kilo::Render {

class TrailBuffer {
public:
    explicit TrailBuffer(size_t maxPoints = 128, double stepMeters = 1.0)
        : maxPoints_(maxPoints), stepMeters_(stepMeters) {}

    // Record a position (ECEF cache — one GeographicLib call per point).
    // Call from worker thread or any thread under panel mutex.
    void Record(double lat, double lon, double alt) {
        auto ecef = GeoRef::ToEcef(lat, lon, alt);
        if (ecefPoints_.empty() || glm::length(ecef - ecefPoints_.back()) > stepMeters_) {
            ecefPoints_.push_back(ecef);
            if (ecefPoints_.size() > maxPoints_) ecefPoints_.pop_front();
        }
    }

    // Draw trail as fading polyline. Call inside Begin/End scope.
    void Draw(const glm::vec4& color, float width = 1.5f) const {
        if (ecefPoints_.size() < 2) return;
        localBuf_.resize(ecefPoints_.size());
        for (size_t i = 0; i < ecefPoints_.size(); ++i)
            localBuf_[i] = glm::vec3(EcefToLocal(ecefPoints_[i]));
        Trail(localBuf_.data(), int(localBuf_.size()), color, width);
    }

    void Clear() { ecefPoints_.clear(); }
    size_t Size() const { return ecefPoints_.size(); }
    bool   Empty() const { return ecefPoints_.empty(); }

    void SetMaxPoints(size_t n) { maxPoints_ = n; }
    void SetStepMeters(double m) { stepMeters_ = m; }

private:
    size_t maxPoints_;
    double stepMeters_;
    std::deque<glm::dvec3>          ecefPoints_;
    mutable std::vector<glm::vec3>  localBuf_;  // reused per frame (avoids alloc)
};

} // namespace Kilo::Render
