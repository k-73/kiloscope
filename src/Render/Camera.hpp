#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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
public:
    // ── orbit mode ──────────────────────────────────────────────
    void Orbit(float dx, float dy, float sensitivity = 0.3f) {
        yaw_   -= dx * sensitivity;
        pitch_  = std::clamp(pitch_ + dy * sensitivity, -89.f, 89.f);
    }

    void Zoom(float delta) {
        dist_ = std::clamp(dist_ * (1.f - delta * 0.1f), 0.01f, 1000.f);
    }

    void Pan(float dx, float dy) {
        float speed = dist_ * 0.002f;
        pivot_ += -Right() * dx * speed + Up() * dy * speed;
    }

    // ── fly mode ────────────────────────────────────────────────
    void FlyLook(float dx, float dy, float sensitivity = 0.15f) {
        auto pos = Eye();
        yaw_   -= dx * sensitivity;
        pitch_  = std::clamp(pitch_ + dy * sensitivity, -89.f, 89.f);
        pivot_  = pos + ViewDir() * dist_;
    }

    void FlyMove(float fwd, float right, float up, float dt) {
        float speed = std::max(dist_, 1.f) * dt;
        glm::vec3 move = ViewDir() * fwd + Right() * right + glm::vec3(0, 0, up);
        pivot_ += move * speed;
    }

    // ── focus ───────────────────────────────────────────────────
    void Focus(const glm::vec3& pos) { pivot_ = pos; }

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
    glm::vec3 pivot_{0.f};
    float dist_ = 8.f, yaw_ = 45.f, pitch_ = 30.f, fov_ = 45.f;
};

} // namespace Kilo::Render
