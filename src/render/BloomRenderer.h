#pragma once

#include "render/Shader.h"

#include <glad/glad.h>

class BloomRenderer {
public:
    BloomRenderer(Shader& extractShader, Shader& blurShader, Shader& compositeShader);
    ~BloomRenderer();

    BloomRenderer(const BloomRenderer&) = delete;
    BloomRenderer& operator=(const BloomRenderer&) = delete;

    bool resize(int width, int height);
    void render(GLuint hdrSceneTexture);

    float exposure = 1.35f;
    float threshold = 1.0f;
    float strength = 0.5f;
    int blurIterations = 8;
    bool enabled = true;

private:
    Shader& extractShader_;
    Shader& blurShader_;
    Shader& compositeShader_;

    GLuint brightFbo_ = 0;
    GLuint brightTexture_ = 0;
    GLuint pingpongFbo_[2] = {0, 0};
    GLuint pingpongTexture_[2] = {0, 0};
    GLuint quadVao_ = 0;
    GLuint quadVbo_ = 0;
    int width_ = 0;
    int height_ = 0;

    void release();
    void ensureQuad();
    void drawQuad();
};
