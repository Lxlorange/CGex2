#pragma once

#include <glm/glm.hpp>

struct Material {
    glm::vec3 ambient{0.2f}, diffuse{0.8f}, specular{0.5f}, emissive{0.0f};
    float shininess = 32.0f;
};
