#include "Fbo.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace Kilo::Render {

Fbo::~Fbo() { Destroy(); }

Fbo::Fbo(Fbo&& o) noexcept
    : w_(std::exchange(o.w_, 0)), h_(std::exchange(o.h_, 0)), samples_(o.samples_)
    , msaaFbo_(std::exchange(o.msaaFbo_, 0))
    , msaaColor_(std::exchange(o.msaaColor_, 0))
    , msaaDepth_(std::exchange(o.msaaDepth_, 0))
    , resolveFbo_(std::exchange(o.resolveFbo_, 0))
    , resolvedTex_(std::exchange(o.resolvedTex_, 0))
{}

Fbo& Fbo::operator=(Fbo&& o) noexcept {
    if (this != &o) {
        Destroy();
        w_ = std::exchange(o.w_, 0); h_ = std::exchange(o.h_, 0); samples_ = o.samples_;
        msaaFbo_     = std::exchange(o.msaaFbo_, 0);
        msaaColor_   = std::exchange(o.msaaColor_, 0);
        msaaDepth_   = std::exchange(o.msaaDepth_, 0);
        resolveFbo_  = std::exchange(o.resolveFbo_, 0);
        resolvedTex_ = std::exchange(o.resolvedTex_, 0);
    }
    return *this;
}

void Fbo::Destroy() {
    if (msaaFbo_)     { glDeleteFramebuffers(1, &msaaFbo_);    msaaFbo_ = 0; }
    if (msaaColor_)   { glDeleteRenderbuffers(1, &msaaColor_); msaaColor_ = 0; }
    if (msaaDepth_)   { glDeleteRenderbuffers(1, &msaaDepth_); msaaDepth_ = 0; }
    if (resolveFbo_)  { glDeleteFramebuffers(1, &resolveFbo_); resolveFbo_ = 0; }
    if (resolvedTex_) { glDeleteTextures(1, &resolvedTex_);    resolvedTex_ = 0; }
}

void Fbo::Resize(int w, int h, int samples) {
    w = std::max(1, w); h = std::max(1, h);
    if (w == w_ && h == h_ && samples == samples_) return;
    Destroy();
    w_ = w; h_ = h;

    GLint maxS = 0; glGetIntegerv(GL_MAX_SAMPLES, &maxS);
    samples_ = std::clamp(samples, 1, std::max(1, (int)maxS));

    glCreateFramebuffers(1, &msaaFbo_);
    glCreateRenderbuffers(1, &msaaColor_);
    glNamedRenderbufferStorageMultisample(msaaColor_, samples_, GL_RGBA16F, w_, h_);
    glNamedFramebufferRenderbuffer(msaaFbo_, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColor_);

    glCreateRenderbuffers(1, &msaaDepth_);
    glNamedRenderbufferStorageMultisample(msaaDepth_, samples_, GL_DEPTH_COMPONENT32F, w_, h_);
    glNamedFramebufferRenderbuffer(msaaFbo_, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepth_);

    glCreateFramebuffers(1, &resolveFbo_);
    glCreateTextures(GL_TEXTURE_2D, 1, &resolvedTex_);
    glTextureStorage2D(resolvedTex_, 1, GL_RGBA16F, w_, h_);
    glTextureParameteri(resolvedTex_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(resolvedTex_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glNamedFramebufferTexture(resolveFbo_, GL_COLOR_ATTACHMENT0, resolvedTex_, 0);

    auto check = [](GLuint fbo, const char* name) {
        if (glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error(std::string("FBO incomplete: ") + name);
    };
    check(msaaFbo_, "msaa");
    check(resolveFbo_, "resolve");
}

void Fbo::Bind(float clearR, float clearG, float clearB) {
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);
    glViewport(0, 0, w_, h_);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glClearColor(clearR, clearG, clearB, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Fbo::Resolve() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_);
    glBlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace Kilo::Render
