#include "render/BloomRenderer.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>

BloomRenderer::BloomRenderer(Shader& extractShader, Shader& blurShader, Shader& compositeShader)
    : extractShader_(extractShader)
    , blurShader_(blurShader)
    , compositeShader_(compositeShader)
{
}

BloomRenderer::~BloomRenderer()
{
    release();
}

void BloomRenderer::release()
{
    if (glfwGetCurrentContext() == nullptr) {
        brightFbo_ = 0;
        brightTexture_ = 0;
        pingpongFbo_[0] = pingpongFbo_[1] = 0;
        pingpongTexture_[0] = pingpongTexture_[1] = 0;
        quadVao_ = 0;
        quadVbo_ = 0;
        return;
    }

    if (quadVbo_ != 0) {
        glDeleteBuffers(1, &quadVbo_);
        quadVbo_ = 0;
    }
    if (quadVao_ != 0) {
        glDeleteVertexArrays(1, &quadVao_);
        quadVao_ = 0;
    }
    if (brightTexture_ != 0) {
        glDeleteTextures(1, &brightTexture_);
        brightTexture_ = 0;
    }
    if (brightFbo_ != 0) {
        glDeleteFramebuffers(1, &brightFbo_);
        brightFbo_ = 0;
    }
    glDeleteTextures(2, pingpongTexture_);
    glDeleteFramebuffers(2, pingpongFbo_);
    pingpongTexture_[0] = pingpongTexture_[1] = 0;
    pingpongFbo_[0] = pingpongFbo_[1] = 0;
}

bool BloomRenderer::resize(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (width == width_ && height == height_ && brightFbo_ != 0) {
        return true;
    }

    release();
    width_ = width;
    height_ = height;

    glGenFramebuffers(1, &brightFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, brightFbo_);
    glGenTextures(1, &brightTexture_);
    glBindTexture(GL_TEXTURE_2D, brightTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brightTexture_, 0);
    bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glGenFramebuffers(2, pingpongFbo_);
    glGenTextures(2, pingpongTexture_);
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFbo_[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongTexture_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongTexture_[i], 0);
        complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ensureQuad();
    if (!complete) {
        std::cerr << "[Bloom] Framebuffer incomplete.\n";
    }
    return complete;
}

void BloomRenderer::ensureQuad()
{
    if (quadVao_ != 0) {
        return;
    }

    const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };

    glGenVertexArrays(1, &quadVao_);
    glGenBuffers(1, &quadVbo_);
    glBindVertexArray(quadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);
}

void BloomRenderer::drawQuad()
{
    glBindVertexArray(quadVao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void BloomRenderer::render(GLuint hdrSceneTexture)
{
    if (hdrSceneTexture == 0 || width_ <= 0 || height_ <= 0) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, width_, height_);

    glBindFramebuffer(GL_FRAMEBUFFER, brightFbo_);
    glClear(GL_COLOR_BUFFER_BIT);
    extractShader_.use();
    extractShader_.setInt("uScene", 0);
    extractShader_.setFloat("uThreshold", threshold);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrSceneTexture);
    drawQuad();

    bool horizontal = true;
    bool firstIteration = true;
    const int iterations = std::clamp(blurIterations, 0, 32);
    blurShader_.use();
    blurShader_.setInt("uImage", 0);
    for (int i = 0; i < iterations; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFbo_[horizontal ? 1 : 0]);
        blurShader_.setBool("uHorizontal", horizontal);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, firstIteration ? brightTexture_ : pingpongTexture_[horizontal ? 0 : 1]);
        drawQuad();
        horizontal = !horizontal;
        firstIteration = false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    compositeShader_.use();
    compositeShader_.setInt("uScene", 0);
    compositeShader_.setInt("uBloomBlur", 1);
    compositeShader_.setBool("uBloomEnabled", enabled);
    compositeShader_.setFloat("uExposure", exposure);
    compositeShader_.setFloat("uBloomStrength", strength);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrSceneTexture);
    glActiveTexture(GL_TEXTURE1);
    const GLuint bloomTex = firstIteration ? brightTexture_ : pingpongTexture_[horizontal ? 0 : 1];
    glBindTexture(GL_TEXTURE_2D, bloomTex);
    drawQuad();
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
}
