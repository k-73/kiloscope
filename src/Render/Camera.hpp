#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Kilo::Render {

class Camera {
public:
    void Orbit(float dx, float dy) {
        yaw_ -= dx * 0.3f;
        pitch_ = std::clamp(pitch_ + dy * 0.3f, -89.f, 89.f);
    }

    void Zoom(float delta) {
        dist_ = std::clamp(dist_ * (1.f - delta * 0.1f), 0.5f, 200.f);
    }

    void Pan(float dx, float dy) {
        float r = glm::radians(yaw_);
        glm::vec3 right(-std::sin(r), std::cos(r), 0.f);
        float speed = dist_ * 0.002f;
        target_ += -right * dx * speed + glm::vec3(0, 0, 1) * dy * speed;
    }

    glm::vec3 Position() const {
        float ry = glm::radians(yaw_), rp = glm::radians(pitch_);
        return target_ + glm::vec3(
            dist_ * std::cos(rp) * std::cos(ry),
            dist_ * std::cos(rp) * std::sin(ry),
            dist_ * std::sin(rp));
    }

    glm::mat4 View() const { return glm::lookAt(Position(), target_, {0, 0, 1}); }

    glm::mat4 Projection(float aspect) const {
        float nr = std::max(0.01f, dist_ * 0.01f);
        float fr = std::max(100.f, dist_ * 50.f);
        return glm::perspective(glm::radians(45.f), aspect, nr, fr);
    }

    float Distance() const { return dist_; }
    glm::vec3 Target() const { return target_; }

private:
    glm::vec3 target_{0.f};
    float dist_ = 8.f, yaw_ = 45.f, pitch_ = 30.f;
};

} // namespace Kilo::Render
