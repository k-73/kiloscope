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
// pivot_ (dvec3) and dist_ (double) give sub-millimeter precision at planetary
// scales.  Direction vectors (yaw/pitch) stay float — unit vectors don't need
// double.  View/Projection matrices are float (GL requirement).
class Camera {
    static constexpr float  kPitchMin = -89.f, kPitchMax = 89.f;
    static constexpr double kDistMin  = 0.01,  kDistMax  = 1e6;

public:
    // ── orbit mode ──────────────────────────────────────────────
    void Orbit(float dx, float dy, float sensitivity = 0.3f) {
        yaw_   -= dx * sensitivity;
        pitch_  = std::clamp(pitch_ + dy * sensitivity, kPitchMin, kPitchMax);
    }

    void Zoom(float delta) {
        dist_ = std::clamp(dist_ * (1.0 - double(delta) * 0.1), kDistMin, kDistMax);
    }

    void Pan(float dx, float dy) {
        float speed = float(dist_) * 0.002f;
        pivot_ += glm::dvec3(-Right() * dx * speed + Up() * dy * speed);
    }

    // ── fly mode ────────────────────────────────────────────────
    void FlyLook(float dx, float dy, float sensitivity = 0.15f) {
        auto pos = Eye();
        yaw_   -= dx * sensitivity;
        pitch_  = std::clamp(pitch_ + dy * sensitivity, kPitchMin, kPitchMax);
        pivot_  = pos + glm::dvec3(ViewDir()) * dist_;
    }

    void FlyMove(float fwd, float right, float up, float dt) {
        double speed = std::max(dist_, 1.0) * double(dt);
        pivot_ += glm::dvec3(ViewDir()) * (double(fwd) * speed)
                + glm::dvec3(Right())   * (double(right) * speed)
                + glm::dvec3(Up())      * (double(up) * speed);
    }

    // ── coordinate frame ─────────────────────────────────────────
    void SetFrame(FrameId id)         { frameMat_ = FrameMat(id); }
    void SetFrame(const glm::mat3& m) { frameMat_ = m; }
    const glm::mat3& GetFrame() const { return frameMat_; }

    // ── positioning (frame-aware) ────────────────────────────────
    void Focus(const glm::vec3& pos) { pivot_ = glm::dvec3(frameMat_ * pos); }

    void LookAt(const glm::vec3& eye, const glm::vec3& target) {
        auto e = frameMat_ * eye, t = frameMat_ * target;
        pivot_ = glm::dvec3(t);
        SetFromDir(e - t);
    }

    void LookDir(const glm::vec3& pos, const glm::vec3& dir, float distance = -1.f) {
        if (distance > 0.f) dist_ = double(distance);
        auto d = glm::normalize(frameMat_ * dir);
        pivot_ = glm::dvec3(frameMat_ * pos) + glm::dvec3(d) * dist_;
        SetFromViewDir(d);
    }

    void SetPose(const glm::vec3& pos, const glm::quat& orientation, float distance = -1.f) {
        LookDir(pos, orientation * glm::vec3(1, 0, 0), distance);
    }

    // ── follow mode ─────────────────────────────────────────────
    void Follow(const glm::vec3& target, float heading, float basePitch = 18.f) {
        following_ = true;
        pivot_ = glm::dvec3(frameMat_ * target);
        float hr = glm::radians(heading);
        auto d = frameMat_ * glm::vec3(std::cos(hr), std::sin(hr), 0.f);
        yaw_   = glm::degrees(std::atan2(-d.y, -d.x)) + followYawOff_;
        pitch_ = basePitch + followPitchOff_;
        dist_  = double(followDist_);
        snapYaw_   = yaw_;
        snapPitch_ = pitch_;
    }

    void CaptureFollow() {
        followYawOff_   += yaw_ - snapYaw_;
        followPitchOff_  = std::clamp(followPitchOff_ + pitch_ - snapPitch_, -70.f, 70.f);
        followDist_      = float(dist_);
    }

    void Unfollow()  { following_ = false; }
    bool Following() const { return following_; }

    void ResetFollow(float distance = -1.f) {
        following_ = true;
        followYawOff_ = followPitchOff_ = 0.f;
        if (distance > 0.f) followDist_ = distance;
    }

    // ── direction vectors (internal XYZ, float — unit vectors) ───
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

    // ── transforms ───────────────────────────────────────────────
    // Eye() returns double — callers that need float should cast explicitly.
    glm::dvec3 Eye() const { return pivot_ - glm::dvec3(ViewDir()) * dist_; }

    glm::mat4 View() const {
        return glm::mat4(glm::lookAt(Eye(), pivot_, glm::dvec3(0, 0, 1)));
    }

    glm::mat4 ViewCamRelative() const {
        return glm::mat4(glm::lookAt(glm::dvec3(0.0), glm::dvec3(ViewDir()) * dist_, glm::dvec3(0, 0, 1)));
    }

    glm::mat4 Projection(float aspect, float autoFar = 10000.f) const {
        float fr = farPlane_  > 0.f ? farPlane_ : autoFar;
        float nr = nearPlane_ > 0.f ? nearPlane_ : std::max(0.01f, float(dist_ * 0.005));
        if (ortho_) {
            float h = float(dist_) * std::tan(glm::radians(fov_ * 0.5f));
            return glm::ortho(-h * aspect, h * aspect, -h, h, nr, fr);
        }
        return glm::perspective(glm::radians(fov_), aspect, nr, fr);
    }

    // ── accessors (frame-aware, float — for panels/GUI) ──────────
    glm::vec3 Position() const { return glm::vec3(glm::transpose(glm::dmat3(frameMat_)) * Eye()); }
    glm::vec3 Pivot()    const { return glm::vec3(glm::transpose(glm::dmat3(frameMat_)) * pivot_); }

    double Distance()  const { return dist_; }
    float  Yaw()       const { return yaw_; }
    float  Pitch()     const { return pitch_; }
    float  Fov()       const { return fov_; }
    bool   Ortho()     const { return ortho_; }
    float  NearPlane() const { return nearPlane_; }
    float  FarPlane()  const { return farPlane_; }

    // ── mutable refs (raw internal — for DragFloat, renderer) ─────
    const glm::dvec3& Target() const { return pivot_; }
    glm::dvec3&       Target()       { return pivot_; }
    double& Distance()  { return dist_; }
    float&  Yaw()       { return yaw_; }
    float&  Pitch()     { return pitch_; }
    float&  Fov()       { return fov_; }
    bool&   Ortho()     { return ortho_; }
    float&  NearPlane() { return nearPlane_; }
    float&  FarPlane()  { return farPlane_; }

private:
    void SetFromDir(const glm::vec3& eyeToTarget) {
        dist_ = double(glm::length(eyeToTarget));
        if (dist_ < 1e-6) return;
        auto d = eyeToTarget / float(dist_);
        SetFromViewDir(-d);
    }

    void SetFromViewDir(const glm::vec3& d) {
        pitch_ = glm::degrees(std::asin(glm::clamp(-d.z, -1.f, 1.f)));
        yaw_   = glm::degrees(std::atan2(-d.y, -d.x));
    }

    glm::mat3  frameMat_  {1.f};
    glm::dvec3 pivot_     {0.0};
    double     dist_      = 8.0;
    float      yaw_       = 45.f;
    float      pitch_     = 30.f;
    float      fov_       = 45.f;
    bool       ortho_     = false;
    float      nearPlane_ = 0.f;
    float      farPlane_  = 0.f;

    bool  following_      = false;
    float followYawOff_   = 0.f;
    float followPitchOff_ = 0.f;
    float followDist_     = 4.f;
    float snapYaw_        = 0.f;
    float snapPitch_      = 0.f;
};

} // namespace Kilo::Render
