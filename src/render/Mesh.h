#pragma once

#include "Shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

struct Vertex {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec2 texCoords {};
};

struct TextureAsset {
    GLuint id = 0;
    std::string type;
    std::string path;
};

class Mesh {
public:
    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<TextureAsset> textures);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw(const Shader& shader) const;
    void attachTextureIfMissing(const TextureAsset& texture);

private:
    std::vector<Vertex> vertices_;
    std::vector<unsigned int> indices_;
    std::vector<TextureAsset> textures_;

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;

    void setupMesh();
    void releaseGlObjects();
};
