#pragma once
#include "Render/Frame.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace Kilo::Render {

// Blender-style orbit camera with FPS fly mode.
//
// Orbit mode:  camera rotates around pivot (target).
// Fly mode:    camera moves freely, pivot follows.
//
// Controls (set up in Draw.cpp Begin):
//   MMB drag     = orbit / pan (+ shift)
//   Scroll       = zoom (change distance to pivot)
//   RMB hold     = fly mode (look + WASD/QE movement)
class Camera {
    static constexpr float kPitchMin = -89.f, kPitchMax = 89.f;
    static constexpr float kDistMin  = 0.01f, kDistMax  = 1000.f;

public:
    // ── orbit mode ──────────────────────────────────────────────
    void Orbit(float dx, float dy, float sensitivity = 0.3f) {
        yaw_   -= dx * sensitivity;
        pitch_  = std::clamp(pitch_ + dy * sensitivity, kPitchMin, kPitchMax);
    }

    void Zoom(float delta) {
        dist_ = std::clamp(dist_ * (1.f - delta * 0.1f), kDistMin, kDistMax);
    }

    void Pan(float dx, float dy) {
        float speed = dist_ * 0.002f;
        pivot_ += -Right() * dx * speed + Up() * dy * speed;
    }

    // ── fly mode ────────────────────────────────────────────────
    void FlyLook(float dx, float dy, float sensitivity = 0.15f) {
        auto pos = Eye();
        yaw_   -= dx * sensitivity;
        pitch_  = std::clamp(pitch_ + dy * sensitivity, kPitchMin, kPitchMax);
        pivot_  = pos + ViewDir() * dist_;
    }

    void FlyMove(float fwd, float right, float up, float dt) {
        float speed = std::max(dist_, 1.f) * dt;
        pivot_ += ViewDir() * fwd * speed + Right() * right * speed
                + Up() * up * speed;
    }

    // ── coordinate frame ─────────────────────────────────────────
    // Positioning methods (Follow, LookAt, LookDir, SetPose, Focus) auto-convert
    // from this frame to internal XYZ. Position()/Pivot() convert back.
    void SetFrame(FrameId id)         { frameMat_ = FrameMat(id); }
    void SetFrame(const glm::mat3& m) { frameMat_ = m; }
    const glm::mat3& GetFrame() const { return frameMat_; }

    // ── positioning (frame-aware) ────────────────────────────────
    void Focus(const glm::vec3& pos) { pivot_ = frameMat_ * pos; }

    void LookAt(const glm::vec3& eye, const glm::vec3& target) {
        auto e = frameMat_ * eye, t = frameMat_ * target;
        pivot_ = t;
        SetFromDir(e - t);
    }

    void LookDir(const glm::vec3& pos, const glm::vec3& dir, float distance = -1.f) {
        if (distance > 0.f) dist_ = distance;
        auto d = glm::normalize(frameMat_ * dir);
        pivot_ = frameMat_ * pos + d * dist_;
        SetFromViewDir(d);
    }

    // Set camera from position + orientation quaternion.
    // Forward direction is derived from orientation * X-axis (+X = forward in body frame).
    void SetPose(const glm::vec3& pos, const glm::quat& orientation, float distance = -1.f) {
        LookDir(pos, orientation * glm::vec3(1, 0, 0), distance);
    }

    // ── follow mode ─────────────────────────────────────────────
    // Camera tracks a moving target with base yaw/pitch while the
    // user can orbit (MMB) and zoom (scroll) on top.
    // Call Follow() before Render::Begin, CaptureFollow() after End.
    void Follow(const glm::vec3& target, float yaw, float basePitch = 18.f) {
        following_ = true;
        pivot_ = frameMat_ * target;
        yaw_   = yaw + followYawOff_;
        pitch_ = basePitch + followPitchOff_;
        dist_  = followDist_;
        snapYaw_   = yaw_;
        snapPitch_ = pitch_;
    }

    void CaptureFollow() {
        followYawOff_   += yaw_ - snapYaw_;
        followPitchOff_  = std::clamp(followPitchOff_ + pitch_ - snapPitch_, -70.f, 70.f);
        followDist_      = dist_;
    }

    void Unfollow()  { following_ = false; }
    bool Following() const { return following_; }

    void ResetFollow(float distance = -1.f) {
        following_ = true;
        followYawOff_ = followPitchOff_ = 0.f;
        if (distance > 0.f) followDist_ = distance;
    }

    // ── direction vectors (internal XYZ) ─────────────────────────
    glm::vec3 ViewDir() const {
        float cy = std::cos(glm::radians(yaw_)),  sy = std::sin(glm::radians(yaw_));
        float cp = std::cos(glm::radians(pitch_)), sp = std::sin(glm::radians(pitch_));
        return {-cp * cy, -cp * sy, -sp};
    }

    glm::vec3 Right() const {
        return glm::normalize(glm::cross(ViewDir(), glm::vec3(0, 0, 1)));
    }

    glm::vec3 Up() const {
        return glm::normalize(glm::cross(Right(), ViewDir()));
    }

    // ── transforms (internal XYZ — used by renderer) ─────────────
    glm::vec3 Eye()  const { return pivot_ - ViewDir() * dist_; }
    glm::mat4 View() const { return glm::lookAt(Eye(), pivot_, {0, 0, 1}); }

    glm::mat4 Projection(float aspect) const {
        float nr = nearPlane_ > 0.f ? nearPlane_ : std::max(0.01f, dist_ * 0.005f);
        float fr = farPlane_  > 0.f ? farPlane_  : 10000.f;
        if (ortho_) {
            float h = dist_ * std::tan(glm::radians(fov_ * 0.5f));
            return glm::ortho(-h * aspect, h * aspect, -h, h, nr, fr);
        }
        return glm::perspective(glm::radians(fov_), aspect, nr, fr);
    }

    // ── accessors (frame-aware — for panels/GUI) ──────────────────
    glm::vec3 Position() const { return glm::transpose(frameMat_) * Eye(); }
    glm::vec3 Pivot()    const { return glm::transpose(frameMat_) * pivot_; }

    float Distance()  const { return dist_; }
    float Yaw()       const { return yaw_; }
    float Pitch()     const { return pitch_; }
    float Fov()       const { return fov_; }
    bool  Ortho()     const { return ortho_; }
    float NearPlane() const { return nearPlane_; }
    float FarPlane()  const { return farPlane_; }

    // ── mutable refs (raw internal — for DragFloat, renderer) ─────
    const glm::vec3& Target() const { return pivot_; }
    glm::vec3&       Target()       { return pivot_; }
    float& Distance()  { return dist_; }
    float& Yaw()       { return yaw_; }
    float& Pitch()     { return pitch_; }
    float& Fov()       { return fov_; }
    bool&  Ortho()     { return ortho_; }
    float& NearPlane() { return nearPlane_; }  // 0 = auto
    float& FarPlane()  { return farPlane_; }   // 0 = auto

private:
    // Decompose eye-to-target vector into distance + yaw/pitch
    void SetFromDir(const glm::vec3& eyeToTarget) {
        dist_ = glm::length(eyeToTarget);
        if (dist_ < 1e-6f) return;
        auto d = eyeToTarget / dist_;
        SetFromViewDir(-d);
    }

    // Set yaw/pitch from a normalized view direction
    void SetFromViewDir(const glm::vec3& d) {
        pitch_ = glm::degrees(std::asin(glm::clamp(-d.z, -1.f, 1.f)));
        yaw_   = glm::degrees(std::atan2(-d.y, -d.x));
    }

    glm::mat3 frameMat_  {1.f};
    glm::vec3 pivot_     {0.f};
    float     dist_      = 8.f;
    float     yaw_       = 45.f;
    float     pitch_     = 30.f;
    float     fov_       = 45.f;
    bool      ortho_     = false;
    float     nearPlane_ = 0.f;     // 0 = auto
    float     farPlane_  = 0.f;     // 0 = auto

    // follow mode
    bool  following_      = false;
    float followYawOff_   = 0.f;
    float followPitchOff_ = 0.f;
    float followDist_     = 4.f;
    float snapYaw_        = 0.f;
    float snapPitch_      = 0.f;
};

} // namespace Kilo::Render
