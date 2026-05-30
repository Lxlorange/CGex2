#include "render/SSAORenderer.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

SSAORenderer::SSAORenderer(Shader& geometryShader, Shader& ssaoShader, Shader& blurShader, Camera& camera)
    : geometryShader_(geometryShader)
    , ssaoShader_(ssaoShader)
    , blurShader_(blurShader)
    , camera_(camera)
{
    createKernelAndNoise();
}

SSAORenderer::~SSAORenderer()
{
    release();
}

void SSAORenderer::release()
{
    if (glfwGetCurrentContext() == nullptr) {
        gBuffer_ = 0;
        gPosition_ = 0;
        gNormal_ = 0;
        depthRbo_ = 0;
        ssaoFbo_ = 0;
        ssaoColorBuffer_ = 0;
        blurFbo_ = 0;
        blurColorBuffer_ = 0;
        noiseTexture_ = 0;
        quadVao_ = 0;
        quadVbo_ = 0;
        return;
    }

    if (quadVbo_ != 0) glDeleteBuffers(1, &quadVbo_);
    if (quadVao_ != 0) glDeleteVertexArrays(1, &quadVao_);
    if (noiseTexture_ != 0) glDeleteTextures(1, &noiseTexture_);
    if (blurColorBuffer_ != 0) glDeleteTextures(1, &blurColorBuffer_);
    if (ssaoColorBuffer_ != 0) glDeleteTextures(1, &ssaoColorBuffer_);
    if (gNormal_ != 0) glDeleteTextures(1, &gNormal_);
    if (gPosition_ != 0) glDeleteTextures(1, &gPosition_);
    if (depthRbo_ != 0) glDeleteRenderbuffers(1, &depthRbo_);
    if (blurFbo_ != 0) glDeleteFramebuffers(1, &blurFbo_);
    if (ssaoFbo_ != 0) glDeleteFramebuffers(1, &ssaoFbo_);
    if (gBuffer_ != 0) glDeleteFramebuffers(1, &gBuffer_);

    gBuffer_ = 0;
    gPosition_ = 0;
    gNormal_ = 0;
    depthRbo_ = 0;
    ssaoFbo_ = 0;
    ssaoColorBuffer_ = 0;
    blurFbo_ = 0;
    blurColorBuffer_ = 0;
    noiseTexture_ = 0;
    quadVao_ = 0;
    quadVbo_ = 0;
}

bool SSAORenderer::resize(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (width == width_ && height == height_ && gBuffer_ != 0) {
        return true;
    }

    release();
    width_ = width;
    height_ = height;
    createKernelAndNoise();

    glGenFramebuffers(1, &gBuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);

    glGenTextures(1, &gPosition_);
    glBindTexture(GL_TEXTURE_2D, gPosition_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition_, 0);

    glGenTextures(1, &gNormal_);
    glBindTexture(GL_TEXTURE_2D, gNormal_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal_, 0);

    const GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    glGenRenderbuffers(1, &depthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo_);
    bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glGenFramebuffers(1, &ssaoFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFbo_);
    glGenTextures(1, &ssaoColorBuffer_);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width_, height_, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer_, 0);
    complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glGenFramebuffers(1, &blurFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, blurFbo_);
    glGenTextures(1, &blurColorBuffer_);
    glBindTexture(GL_TEXTURE_2D, blurColorBuffer_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width_, height_, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurColorBuffer_, 0);
    complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ensureQuad();

    if (!complete) {
        std::cerr << "[SSAO] Framebuffer incomplete.\n";
    }
    return complete;
}

void SSAORenderer::render(const Scene& scene)
{
    if (!enabled || !isReady()) {
        return;
    }

    const float aspect = static_cast<float>(std::max(width_, 1)) / static_cast<float>(std::max(height_, 1));
    const glm::mat4 projection = glm::perspective(glm::radians(camera_.Zoom), aspect, 0.1f, 2000.0f);
    const glm::mat4 view = camera_.GetViewMatrix();

    glViewport(0, 0, width_, height_);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    geometryShader_.use();
    geometryShader_.setMat4("uProjection", projection);
    geometryShader_.setMat4("uView", view);
    scene.drawAll(geometryShader_, true);

    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFbo_);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    ssaoShader_.use();
    ssaoShader_.setInt("gPosition", 0);
    ssaoShader_.setInt("gNormal", 1);
    ssaoShader_.setInt("texNoise", 2);
    ssaoShader_.setMat4("uProjection", projection);
    ssaoShader_.setVec2("uNoiseScale", glm::vec2(width_ / 4.0f, height_ / 4.0f));
    ssaoShader_.setFloat("uRadius", std::max(radius, 0.001f));
    ssaoShader_.setFloat("uBias", std::max(bias, 0.0f));
    for (int i = 0; i < static_cast<int>(kernel_.size()); ++i) {
        ssaoShader_.setVec3("uSamples[" + std::to_string(i) + "]", kernel_[static_cast<std::size_t>(i)]);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosition_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noiseTexture_);
    drawQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, blurFbo_);
    glClear(GL_COLOR_BUFFER_BIT);
    blurShader_.use();
    blurShader_.setInt("uSSAOInput", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer_);
    drawQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
}

void SSAORenderer::ensureQuad()
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

void SSAORenderer::drawQuad() const
{
    glBindVertexArray(quadVao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void SSAORenderer::createKernelAndNoise()
{
    std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
    std::default_random_engine generator(42);

    for (int i = 0; i < static_cast<int>(kernel_.size()); ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = static_cast<float>(i) / static_cast<float>(kernel_.size());
        scale = 0.1f + 0.9f * scale * scale;
        kernel_[static_cast<std::size_t>(i)] = sample * scale;
    }

    std::array<glm::vec3, 16> noise{};
    for (glm::vec3& value : noise) {
        value = glm::vec3(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            0.0f);
    }

    glGenTextures(1, &noiseTexture_);
    glBindTexture(GL_TEXTURE_2D, noiseTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
}
