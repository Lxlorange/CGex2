#include "render/Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

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

    scene.drawAll(shader_);
}
