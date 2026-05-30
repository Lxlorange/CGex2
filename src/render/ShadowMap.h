#pragma once

#include <glad/glad.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>

class ShadowMap {
public:
    ShadowMap(int width, int height);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ShadowMap(ShadowMap&&) = delete;
    ShadowMap& operator=(ShadowMap&&) = delete;

    int width() const { return width_; }
    int height() const { return height_; }
    GLuint depthTexture() const { return depthTexture_; }

    void bindWrite();
    void bindRead(GLuint textureUnit);

    static glm::mat4 computeLightSpaceMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax,
        const glm::vec3& lightDirectionUniform);
    static glm::mat4 computeLightSpaceMatrixFromFrustum(const std::array<glm::vec3, 8>& frustumCorners,
        const glm::vec3& lightDirectionUniform);

private:
    int width_;
    int height_;
    GLuint fbo_ = 0;
    GLuint depthTexture_ = 0;
};
