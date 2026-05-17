#pragma once

#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

constexpr float YAW = -90.0f;
constexpr float PITCH = 0.0f;
constexpr float SPEED = 2.5f;
constexpr float SENSITIVITY = 0.06f;
constexpr float ZOOM = 45.0f;

class Camera {
public:
    glm::vec3 Position, Front, Up, Right, WorldUp;
    float Yaw, Pitch, MovementSpeed, MouseSensitivity, Zoom;

    explicit Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
                    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
                    float yaw = YAW, float pitch = PITCH)
        : Position(position), Front(glm::vec3(0.0f, 0.0f, -1.0f)), WorldUp(up)
        , Yaw(yaw), Pitch(pitch), MovementSpeed(SPEED)
        , MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
    { updateCameraVectors(); }

    glm::mat4 GetViewMatrix() const { return glm::lookAt(Position, Position + Front, Up); }

    void ProcessKeyboard(Camera_Movement dir, float dt)
    {
        float v = MovementSpeed * dt;
        glm::vec3 f = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
        glm::vec3 r = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));
        if (dir == Camera_Movement::FORWARD)  Position += f * v;
        if (dir == Camera_Movement::BACKWARD) Position -= f * v;
        if (dir == Camera_Movement::LEFT)     Position -= r * v;
        if (dir == Camera_Movement::RIGHT)    Position += r * v;
        if (dir == Camera_Movement::UP)       Position.y += v;
        if (dir == Camera_Movement::DOWN)     Position.y -= v;
    }

    void ProcessMouseMovement(float xoff, float yoff, bool clamp = true)
    {
        xoff *= MouseSensitivity; yoff *= MouseSensitivity;
        Yaw += xoff; Pitch += yoff;
        if (clamp) { if (Pitch > 89.0f) Pitch = 89.0f; if (Pitch < -89.0f) Pitch = -89.0f; }
        updateCameraVectors();
    }

    void ProcessZoom(float offset)
    {
        Zoom -= offset;
        if (Zoom < 1.0f) Zoom = 1.0f;
        if (Zoom > 60.0f) Zoom = 60.0f;
    }

    void fitFreeFlyViewAroundCenter(const glm::vec3& center, float boundingRadius)
    {
        const float r = std::max(boundingRadius, 0.5f);
        const float fov = glm::radians(std::clamp(Zoom, 20.0f, 60.0f));
        const float distance = std::max((r / std::tan(fov * 0.5f)) * 1.25f, 3.0f);
        const glm::vec3 viewFrom = glm::normalize(glm::vec3(0.85f, 0.38f, 0.85f));
        Position = center + viewFrom * distance;
        lookAt(center);
        MovementSpeed = std::clamp(r * 0.35f, 1.5f, 25.0f);
    }

private:
    void lookAt(const glm::vec3& target)
    {
        const glm::vec3 direction = glm::normalize(target - Position);
        Yaw = glm::degrees(std::atan2(direction.z, direction.x));
        Pitch = glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
        updateCameraVectors();
    }

    void updateCameraVectors()
    {
        glm::vec3 f;
        f.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        f.y = sin(glm::radians(Pitch));
        f.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(f);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};
