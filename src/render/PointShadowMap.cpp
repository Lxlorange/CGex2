#include "render/PointShadowMap.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

PointShadowMap::PointShadowMap(int size)
    : size_(size)
{
    glGenFramebuffers(1, &fbo_);
    glGenTextures(1, &depthCubemap_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap_);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT24,
            size_, size_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X, depthCubemap_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[PointShadow] Framebuffer incomplete.\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

PointShadowMap::~PointShadowMap()
{
    if (glfwGetCurrentContext() == nullptr) {
        fbo_ = 0;
        depthCubemap_ = 0;
        return;
    }
    if (depthCubemap_ != 0) {
        glDeleteTextures(1, &depthCubemap_);
        depthCubemap_ = 0;
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
}

void PointShadowMap::bindFace(int face)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, depthCubemap_, 0);
    glViewport(0, 0, size_, size_);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void PointShadowMap::bindRead(GLuint textureUnit) const
{
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap_);
}

std::array<glm::mat4, 6> PointShadowMap::matrices(const glm::vec3& lightPos, float farPlane) const
{
    const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.05f, farPlane);
    return {
        proj * glm::lookAt(lightPos, lightPos + glm::vec3( 1,  0,  0), glm::vec3(0, -1,  0)),
        proj * glm::lookAt(lightPos, lightPos + glm::vec3(-1,  0,  0), glm::vec3(0, -1,  0)),
        proj * glm::lookAt(lightPos, lightPos + glm::vec3( 0,  1,  0), glm::vec3(0,  0,  1)),
        proj * glm::lookAt(lightPos, lightPos + glm::vec3( 0, -1,  0), glm::vec3(0,  0, -1)),
        proj * glm::lookAt(lightPos, lightPos + glm::vec3( 0,  0,  1), glm::vec3(0, -1,  0)),
        proj * glm::lookAt(lightPos, lightPos + glm::vec3( 0,  0, -1), glm::vec3(0, -1,  0)),
    };
}
