#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <array>

class PointShadowMap {
public:
    explicit PointShadowMap(int size = 512);
    ~PointShadowMap();

    PointShadowMap(const PointShadowMap&) = delete;
    PointShadowMap& operator=(const PointShadowMap&) = delete;
    PointShadowMap(PointShadowMap&&) = delete;
    PointShadowMap& operator=(PointShadowMap&&) = delete;

    void bindFace(int face);
    void bindRead(GLuint textureUnit) const;

    std::array<glm::mat4, 6> matrices(const glm::vec3& lightPos, float farPlane) const;

    int size() const { return size_; }
    GLuint depthCubemap() const { return depthCubemap_; }

private:
    int size_ = 512;
    GLuint fbo_ = 0;
    GLuint depthCubemap_ = 0;
};
