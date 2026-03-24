#pragma once
#include "Core/Panel/Panel.hpp"
#include <glm/glm.hpp>

namespace Kilo {

class Airspace : public Panel {
public:
    Airspace();
    void OnDraw() override;

private:
    void DrawControls();
    void HandleInput(float dt, bool focused);
    void UpdatePhysics(float dt, bool focused);
    void DrawWorld(const char* scene);
    void SetupEnv(const char* scene);

    // Aircraft state
    glm::vec3 pos_{0, 0, -2.f};
    float yaw_   = 0.f;
    float pitch_ = 0.f;
    float roll_  = 0.f;
    float speed_ = 0.f;

    // Camera mode
    bool chase_   = true;
    bool freecam_ = false;
};

} // namespace Kilo
