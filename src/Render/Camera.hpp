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
//   LMB drag     = orbit around pivot
//   MMB drag     = pan (shift pivot)
//   Scroll       = zoom (change distance to pivot)
//   RMB hold     = fly mode (look + WASD/QE movement)
//   Focus(pos)   = animate pivot to position
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
        glm::vec3 move = ViewDir() * fwd + Right() * right + glm::vec3(0, 0, up);
        pivot_ += move * speed;
    }

    // ── coordinate frame ─────────────────────────────────────────
    // High-level methods (Follow, LookAt, Focus) auto-convert positions
    // from this frame to internal XYZ. Low-level accessors (Target(),
    // Distance(), Yaw(), Pitch()) operate in raw internal coordinates.
    void SetFrame(FrameId id)         { frameMat_ = FrameMat(id); }
    void SetFrame(const glm::mat3& m) { frameMat_ = m; }

    // ── focus ───────────────────────────────────────────────────
    void Focus(const glm::vec3& pos) { pivot_ = frameMat_ * pos; }

    void LookAt(const glm::vec3& eye, const glm::vec3& target) {
        auto e = frameMat_ * eye, t = frameMat_ * target;
        pivot_ = t;
        auto d = e - t;
        dist_  = glm::length(d);
        if (dist_ < 1e-6f) return;
        d /= dist_;
        pitch_ = glm::degrees(std::asin(glm::clamp(d.z, -1.f, 1.f)));
        yaw_   = glm::degrees(std::atan2(d.y, d.x));
    }

    void LookDir(const glm::vec3& pos, const glm::vec3& dir) {
        auto p = frameMat_ * pos;
        auto d = glm::normalize(frameMat_ * dir);
        pivot_ = p + d * dist_;
        pitch_ = glm::degrees(std::asin(glm::clamp(-d.z, -1.f, 1.f)));
        yaw_   = glm::degrees(std::atan2(-d.y, -d.x));
    }

    void SetPose(const glm::vec3& pos, const glm::quat& orientation) {
        auto fwd = glm::normalize(frameMat_ * (orientation * glm::vec3(1, 0, 0)));
        auto p   = frameMat_ * pos;
        pivot_ = p + fwd * dist_;
        pitch_ = glm::degrees(std::asin(glm::clamp(-fwd.z, -1.f, 1.f)));
        yaw_   = glm::degrees(std::atan2(-fwd.y, -fwd.x));
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

    // ── direction vectors ───────────────────────────────────────
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

    // ── transforms ──────────────────────────────────────────────
    glm::vec3 Eye() const { return pivot_ - ViewDir() * dist_; }

    glm::mat4 View() const { return glm::lookAt(Eye(), pivot_, {0, 0, 1}); }

    glm::mat4 Projection(float aspect) const {
        float nr = std::max(0.01f, dist_ * 0.005f);
        float fr = std::max(500.f, dist_ * 100.f);
        return glm::perspective(glm::radians(fov_), aspect, nr, fr);
    }

    // ── accessors ───────────────────────────────────────────────
    glm::vec3        Position() const { return Eye(); }
    const glm::vec3& Pivot()    const { return pivot_; }
    float            Distance() const { return dist_; }
    float            Yaw()      const { return yaw_; }
    float            Pitch()    const { return pitch_; }
    float            Fov()      const { return fov_; }

    glm::vec3& Target()        { return pivot_; }
    float&     Distance()      { return dist_; }
    float&     Yaw()           { return yaw_; }
    float&     Pitch()         { return pitch_; }
    float&     Fov()           { return fov_; }

private:
    glm::mat3 frameMat_{1.f};
    glm::vec3 pivot_{0.f};
    float dist_  = 8.f;
    float yaw_   = 45.f;
    float pitch_ = 30.f;
    float fov_   = 45.f;

    // follow mode
    bool  following_      = false;
    float followYawOff_   = 0.f;
    float followPitchOff_ = 0.f;
    float followDist_     = 4.f;
    float snapYaw_        = 0.f;
    float snapPitch_      = 0.f;
};

} // namespace Kilo::Render
