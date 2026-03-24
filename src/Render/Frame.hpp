#pragma once
// Coordinate frame conventions for robotics visualization.
// Internal frame: X-right, Y-forward, Z-up (right-handed).

#include <glm/glm.hpp>

namespace Kilo::Render {

// ── frame tag types ─────────────────────────────────────────────────
// Each carries a constexpr 3x3 matrix mapping its axes to internal axes.
// Column i = where frame's i-th axis lands in internal space.

struct XYZ { static constexpr glm::mat3 M{1.f}; };                                          // identity (internal)
struct ENU { static constexpr glm::mat3 M{1.f}; };                                          // East-North-Up = internal
struct NED { static constexpr glm::mat3 M{{0,1,0}, {1,0,0}, {0,0,-1}}; };                   // North-East-Down
struct FLU { static constexpr glm::mat3 M{{0,1,0}, {-1,0,0}, {0,0,1}}; };                   // Forward-Left-Up (ROS body)
struct FRD { static constexpr glm::mat3 M{{0,1,0}, {1,0,0}, {0,0,-1}}; };                   // Forward-Right-Down (≡ NED: same mapping, different semantics — world vs body frame)

// ── runtime enum mirror ─────────────────────────────────────────────

enum class FrameId : uint8_t { XYZ, ENU, NED, FLU, FRD };

inline constexpr glm::mat3 FrameMat(FrameId id) {
    constexpr glm::mat3 lut[] = {XYZ::M, ENU::M, NED::M, FLU::M, FRD::M};
    return lut[static_cast<int>(id)];
}

// ── compile-time helpers ────────────────────────────────────────────

// Convert a position from frame F to internal (XYZ) coordinates
template<typename F> constexpr glm::vec3 ToInternal(const glm::vec3& v) { return F::M * v; }
template<typename F> constexpr glm::vec3 ToInternal(float x, float y, float z) { return F::M * glm::vec3{x,y,z}; }

template<typename F> constexpr glm::vec3 MapVec(float x, float y, float z) { return F::M * glm::vec3{x,y,z}; }
template<typename F> constexpr glm::vec3 MapVec(const glm::vec3& v)        { return F::M * v; }
template<typename F> constexpr glm::vec3 AxisX() { return F::M[0]; }
template<typename F> constexpr glm::vec3 AxisY() { return F::M[1]; }
template<typename F> constexpr glm::vec3 AxisZ() { return F::M[2]; }

} // namespace Kilo::Render
