#pragma once
#include "Shader.hpp"
#include <glm/glm.hpp>

namespace KiloScope::Render {

class Grid {
public:
    ~Grid() { if (vao_) glDeleteVertexArrays(1, &vao_); }

    void Init(const std::string& dir) {
        shader_ = Shader(dir + "/Grid.vert", dir + "/Grid.frag");
        glCreateVertexArrays(1, &vao_);
    }

    void Draw(const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& camPos, float camDist) {
        shader_.Use();
        shader_.Set("uView", view);
        shader_.Set("uProj", proj);
        shader_.Set("uCamPos", camPos);
        shader_.Set("uCamDist", camDist);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }

private:
    Shader shader_;
    GLuint vao_ = 0;
};

} // namespace KiloScope::Render
