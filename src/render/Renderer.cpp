#include "render/Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
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

void Renderer::render(const Scene& scene)
{
    shader_.use();
    int w = 0, h = 0;
    GLFWwindow* win = glfwGetCurrentContext();
    if (win) glfwGetFramebufferSize(win, &w, &h);
    if (h <= 0) h = 1;

    shader_.setMat4("uProjection", glm::perspective(
        glm::radians(camera_.Zoom),
        static_cast<float>(w) / static_cast<float>(h), 0.1f, 300.0f));
    shader_.setMat4("uView", camera_.GetViewMatrix());
    shader_.setVec3("uLightDirection", lightDir_);
    shader_.setVec3("uViewPosition", camera_.Position);

    shader_.setBool("uLightOn", lightOn_);
    shader_.setFloat("uAmbientStrength", ambientStrength_);
    shader_.setVec3("uAmbientColor", ambientColor_);

    scene.drawAll(shader_);
}
