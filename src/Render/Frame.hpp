#pragma once
// Coordinate frame conventions for robotics visualization.
// Internal frame: X-right, Y-forward, Z-up (right-handed).

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

// ── conversions ────────────────────────────────────────────────────
//   ToInternal<NED>(v)   — convert v from NED to internal XYZ
//   FromInternal<NED>(v) — convert v from internal XYZ to NED

template<typename F> constexpr glm::vec3 ToInternal(const glm::vec3& v)   { return F::M * v; }
template<typename F> inline glm::vec3 FromInternal(const glm::vec3& v) { return glm::transpose(F::M) * v; }

inline constexpr glm::vec3 ToInternal  (FrameId id, const glm::vec3& v) { return FrameMat(id) * v; }
inline glm::vec3 FromInternal(FrameId id, const glm::vec3& v) { return glm::transpose(FrameMat(id)) * v; }

// ── frame axis queries ─────────────────────────────────────────────

template<typename F> constexpr glm::vec3 AxisX() { return F::M[0]; }
template<typename F> constexpr glm::vec3 AxisY() { return F::M[1]; }
template<typename F> constexpr glm::vec3 AxisZ() { return F::M[2]; }

// ── Euler rotations ───────────────────────────────────────────────
// ZYX intrinsic: yaw(Z) → pitch(Y) → roll(X). Standard aerospace body→NED.
inline glm::mat3 EulerZYX(float yawDeg, float pitchDeg, float rollDeg) {
    return glm::mat3(glm::rotate(glm::rotate(glm::rotate(
        glm::mat4(1.f),
        glm::radians(yawDeg),   glm::vec3(0, 0, 1)),
        glm::radians(pitchDeg), glm::vec3(0, 1, 0)),
        glm::radians(rollDeg),  glm::vec3(1, 0, 0)));
}

} // namespace Kilo::Render
