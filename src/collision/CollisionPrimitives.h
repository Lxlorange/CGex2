#pragma once

#include "collision/AABB.h"

#include <glm/glm.hpp>

#include <array>

struct Capsule {
    glm::vec3 segmentA{0.0f};
    glm::vec3 segmentB{0.0f, 1.0f, 0.0f};
    float radius = 0.25f;

    Capsule() = default;
    Capsule(const glm::vec3& a, const glm::vec3& b, float r) noexcept
        : segmentA(a)
        , segmentB(b)
        , radius(r)
    {
    }
};

struct OBB {
    glm::vec3 center{0.0f};
    glm::vec3 halfExtents{0.5f};
    std::array<glm::vec3, 3> axis{
        glm::vec3{1.0f, 0.0f, 0.0f},
        glm::vec3{0.0f, 1.0f, 0.0f},
        glm::vec3{0.0f, 0.0f, 1.0f},
    };

    static OBB fromAABB(const AABB& box) noexcept;
    static OBB fromLocalAABB(const glm::vec3& localMin, const glm::vec3& localMax,
                             const glm::mat4& transform) noexcept;
    std::array<glm::vec3, 8> corners() const noexcept;
};

struct NamedOBB {
    std::string name;
    OBB box;
};

struct Contact {
    bool hit = false;
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec3 point{0.0f};
    float penetration = 0.0f;
};

bool intersect(const OBB& a, const OBB& b) noexcept;
Contact capsuleAABBContact(const Capsule& capsule, const AABB& box) noexcept;
Contact capsuleOBBContact(const Capsule& capsule, const OBB& box) noexcept;

glm::vec3 slideVector(const glm::vec3& velocity, const glm::vec3& normal) noexcept;
