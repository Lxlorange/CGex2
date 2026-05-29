#pragma once

#include <glad/glad.h>

class HDRFramebuffer {
public:
    HDRFramebuffer() = default;
    ~HDRFramebuffer();

    HDRFramebuffer(const HDRFramebuffer&) = delete;
    HDRFramebuffer& operator=(const HDRFramebuffer&) = delete;

    bool resize(int width, int height);
    void bind();
    static void bindDefault();

    GLuint framebuffer() const { return fbo_; }
    GLuint colorTexture() const { return colorTexture_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool isValid() const { return fbo_ != 0 && colorTexture_ != 0 && depthRbo_ != 0; }

private:
    GLuint fbo_ = 0;
    GLuint colorTexture_ = 0;
    GLuint depthRbo_ = 0;
    int width_ = 0;
    int height_ = 0;

    void release();
};
