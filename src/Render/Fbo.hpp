#pragma once
#include <GL/glew.h>

namespace Kilo::Render {

class Fbo {
public:
    Fbo() = default;
    ~Fbo();

    Fbo(Fbo&& o) noexcept;
    Fbo& operator=(Fbo&& o) noexcept;
    Fbo(const Fbo&) = delete;
    Fbo& operator=(const Fbo&) = delete;

    void Resize(int w, int h, int samples = 8);
    void Bind();
    void Resolve();

    GLuint Texture() const { return resolvedTex_; }
    int Width() const { return w_; }
    int Height() const { return h_; }

private:
    void Destroy();
    int w_ = 0, h_ = 0, samples_ = 8;
    GLuint msaaFbo_ = 0, msaaColor_ = 0, msaaDepth_ = 0;
    GLuint resolveFbo_ = 0, resolvedTex_ = 0;
};

} // namespace Kilo::Render
