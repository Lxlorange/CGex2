#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "core/Camera.h"
#include "render/Shader.h"
#include "scene/Scene.h"

#include <array>
#include <vector>

class SSAORenderer {
public:
    SSAORenderer(Shader& geometryShader, Shader& ssaoShader, Shader& blurShader, Camera& camera);
    ~SSAORenderer();

    SSAORenderer(const SSAORenderer&) = delete;
    SSAORenderer& operator=(const SSAORenderer&) = delete;
    SSAORenderer(SSAORenderer&&) = delete;
    SSAORenderer& operator=(SSAORenderer&&) = delete;

    bool enabled = true;
    float radius = 0.75f;
    float bias = 0.025f;
    float strength = 1.0f;

    bool resize(int width, int height);
    void render(const Scene& scene);
    GLuint occlusionTexture() const noexcept { return blurColorBuffer_; }
    bool isReady() const noexcept { return width_ > 0 && height_ > 0 && blurColorBuffer_ != 0; }

private:
    Shader& geometryShader_;
    Shader& ssaoShader_;
    Shader& blurShader_;
    Camera& camera_;

    int width_ = 0;
    int height_ = 0;

    GLuint gBuffer_ = 0;
    GLuint gPosition_ = 0;
    GLuint gNormal_ = 0;
    GLuint depthRbo_ = 0;
    GLuint ssaoFbo_ = 0;
    GLuint ssaoColorBuffer_ = 0;
    GLuint blurFbo_ = 0;
    GLuint blurColorBuffer_ = 0;
    GLuint noiseTexture_ = 0;
    GLuint quadVao_ = 0;
    GLuint quadVbo_ = 0;

    std::array<glm::vec3, 64> kernel_{};

    void release();
    void ensureQuad();
    void drawQuad() const;
    void createKernelAndNoise();
};
