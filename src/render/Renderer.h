#pragma once

#include <glm/glm.hpp>
#include "render/Shader.h"
#include "core/Camera.h"
#include "scene/Scene.h"

class Renderer {
public:
    Renderer(Shader& shader, Camera& camera) : shader_(shader), camera_(camera) {}

    void setLightDirection(const glm::vec3& dir) { lightDir_ = glm::normalize(dir); }
    void setFallbackColor(const glm::vec3& color) { fallbackColor_ = color; }

    void toggleLight() { lightOn_ = !lightOn_; }
    bool isLightOn() const { return lightOn_; }
    void toggleDayNight();
    bool isDayMode() const { return dayMode_; }

    void render(const Scene& scene);

private:
    Shader& shader_;
    Camera& camera_;
    glm::vec3 lightDir_{-0.8f, -1.0f, -0.3f};
    glm::vec3 fallbackColor_{0.8f, 0.8f, 0.8f};
    bool lightOn_ = true;
    bool dayMode_ = true;
    float ambientStrength_ = 0.25f;
    glm::vec3 ambientColor_{1.0f};
};
