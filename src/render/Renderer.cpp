#include "render/Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>

void Renderer::toggleDayNight()
{
    dayMode_ = !dayMode_;
    if (dayMode_) {
        ambientStrength_ = 0.25f;
        ambientColor_ = glm::vec3(1.0f, 1.0f, 1.0f);
        std::cout << "[Renderer] Day mode\n";
    } else {
        ambientStrength_ = 0.06f;
        ambientColor_ = glm::vec3(0.4f, 0.5f, 0.8f);
        std::cout << "[Renderer] Night mode\n";
    }
}

Renderer::Renderer(Shader& litShader, Shader& depthShader, Camera& camera)
    : litShader_(litShader)
    , depthShader_(depthShader)
    , camera_(camera)
    , shadowMap_(2048, 2048)
{
}

void Renderer::setRenderTarget(GLuint framebuffer, int width, int height)
{
    targetFramebuffer_ = framebuffer;
    targetWidth_ = width;
    targetHeight_ = height;
}

void Renderer::render(const Scene& scene)
{
    static bool printedShadowStatus = false;

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);

    if (!sceneBoundsTried_) {
        sceneBoundsValid_ = scene.computeWorldBounds(sceneBmin_, sceneBmax_);
        sceneBoundsTried_ = true;
    }
    const bool haveBounds = sceneBoundsValid_;

    const bool wantShadowPass = directionalShadowsEnabled_ && haveBounds && depthShader_.isValid();
    if (!printedShadowStatus) {
        if (wantShadowPass) {
            std::cout << "[Shadow] Directional shadow mapping enabled. Resolution: "
                      << shadowMap_.width() << "x" << shadowMap_.height() << '\n';
        } else {
            std::cerr << "[Shadow] Directional shadow mapping disabled.\n"
                      << "  directionalShadowsEnabled: " << (directionalShadowsEnabled_ ? "true" : "false") << '\n'
                      << "  sceneBoundsValid: " << (haveBounds ? "true" : "false") << '\n'
                      << "  depthShaderValid: " << (depthShader_.isValid() ? "true" : "false") << '\n';
        }
        printedShadowStatus = true;
    }

    glm::mat4 lightSpace(1.0f);
    if (wantShadowPass) {
        lightSpace = ShadowMap::computeLightSpaceMatrix(sceneBmin_, sceneBmax_, glm::normalize(lightDir_));
    }

    const int pointShadowCount = (lightManager_ != nullptr && pointDepthShader_ != nullptr && pointDepthShader_->isValid())
        ? std::min({static_cast<int>(lightManager_->pointLights.size()), kMaxPointShadowMaps, 8})
        : 0;
    while (static_cast<int>(pointShadowMaps_.size()) < pointShadowCount) {
        pointShadowMaps_.push_back(std::make_unique<PointShadowMap>(384));
    }

    if (wantShadowPass) {
        const GLboolean cullFaceWasEnabled = glIsEnabled(GL_CULL_FACE);
        GLint previousCullFace = GL_BACK;
        glGetIntegerv(GL_CULL_FACE_MODE, &previousCullFace);

        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.25f, 3.0f);

        shadowMap_.bindWrite();
        depthShader_.use();
        depthShader_.setMat4("uLightSpaceMatrix", lightSpace);
        scene.drawAll(depthShader_, true);

        glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer_);
        if (cullFaceWasEnabled) {
            glCullFace(previousCullFace);
        }
        if (cullFaceWasEnabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
        if (targetWidth_ > 0 && targetHeight_ > 0) {
            glViewport(0, 0, targetWidth_, targetHeight_);
        } else {
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        }
    }

    if (pointShadowCount > 0) {
        const GLboolean cullFaceWasEnabled = glIsEnabled(GL_CULL_FACE);
        GLint previousCullFace = GL_BACK;
        glGetIntegerv(GL_CULL_FACE_MODE, &previousCullFace);
        glDisable(GL_CULL_FACE);

        pointDepthShader_->use();
        pointDepthShader_->setFloat("uFarPlane", pointShadowFarPlane_);
        for (int lightIndex = 0; lightIndex < pointShadowCount; ++lightIndex) {
            const glm::vec3 lightPos = lightManager_->pointLights[lightIndex].position;
            pointDepthShader_->setVec3("uLightPos", lightPos);
            const auto matrices = pointShadowMaps_[lightIndex]->matrices(lightPos, pointShadowFarPlane_);
            for (int face = 0; face < 6; ++face) {
                pointShadowMaps_[lightIndex]->bindFace(face);
                pointDepthShader_->setMat4("uShadowMatrix", matrices[face]);
                scene.drawAll(*pointDepthShader_, true, nullptr, true);
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer_);
        if (cullFaceWasEnabled) {
            glCullFace(previousCullFace);
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
        if (targetWidth_ > 0 && targetHeight_ > 0) {
            glViewport(0, 0, targetWidth_, targetHeight_);
        } else {
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        }
    }

    litShader_.use();
    int w = targetWidth_ > 0 ? targetWidth_ : viewport[2];
    int h = targetHeight_ > 0 ? targetHeight_ : viewport[3];
    if (w <= 0 || h <= 0) {
        GLFWwindow* win = glfwGetCurrentContext();
        if (win) glfwGetFramebufferSize(win, &w, &h);
    }
    if (h <= 0) h = 1;

    litShader_.setMat4("uProjection", glm::perspective(
        glm::radians(camera_.Zoom),
        static_cast<float>(w) / static_cast<float>(h), 0.1f, 2000.0f));
    litShader_.setMat4("uView", camera_.GetViewMatrix());
    litShader_.setVec3("uViewPosition", camera_.Position);

    const glm::vec3 dir = glm::normalize(lightDir_);
    litShader_.setVec3("dirLight.direction", dir);
    litShader_.setMat4("uLightSpaceMatrix", lightSpace);
    litShader_.setInt("uUseDirectionalShadow", wantShadowPass ? 1 : 0);
    litShader_.setFloat("uShadowStrength", shadowStrength_);
    litShader_.setInt("shadowMap", kShadowMapUnit);
    litShader_.setVec2("uShadowTexelSize", glm::vec2(
        1.0f / static_cast<float>(shadowMap_.width()),
        1.0f / static_cast<float>(shadowMap_.height())));
    if (sceneBoundsValid_) {
        litShader_.setVec3("uSceneMin", sceneBmin_);
        litShader_.setVec3("uSceneMax", sceneBmax_);
    } else {
        litShader_.setVec3("uSceneMin", glm::vec3(-1.0f));
        litShader_.setVec3("uSceneMax", glm::vec3(1.0f));
    }
    litShader_.setInt("uApplyWindowFalloff", directionalShadowsEnabled_ && sceneBoundsValid_ ? 1 : 0);
    shadowMap_.bindRead(static_cast<GLuint>(kShadowMapUnit));
    litShader_.setInt("uPointShadowCount", pointShadowCount);
    litShader_.setFloat("uPointShadowFarPlane", pointShadowFarPlane_);
    for (int i = 0; i < kMaxPointShadowMaps; ++i) {
        litShader_.setInt("pointShadowMaps[" + std::to_string(i) + "]", kPointShadowBaseUnit + i);
    }
    for (int i = 0; i < pointShadowCount; ++i) {
        pointShadowMaps_[i]->bindRead(static_cast<GLuint>(kPointShadowBaseUnit + i));
    }

    if (lightManager_ != nullptr) {
        lightManager_->sendToShader(litShader_.id());
    } else {
        litShader_.setVec3("globalAmbient", ambientColor_ * ambientStrength_);
        litShader_.setInt("numPointLights", 0);
    }

    scene.drawAll(litShader_, false, &camera_.Position);
}
