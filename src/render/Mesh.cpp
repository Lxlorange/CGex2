#include "Mesh.h"

#include <cstddef>
#include <utility>

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<TextureAsset> textures)
    : vertices_(std::move(vertices))
    , indices_(std::move(indices))
    , textures_(std::move(textures))
{
    setupMesh();
}

Mesh::~Mesh()
{
    releaseGlObjects();
}

Mesh::Mesh(Mesh&& other) noexcept
    : vertices_(std::move(other.vertices_))
    , indices_(std::move(other.indices_))
    , textures_(std::move(other.textures_))
    , vao_(std::exchange(other.vao_, 0))
    , vbo_(std::exchange(other.vbo_, 0))
    , ebo_(std::exchange(other.ebo_, 0))
{
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other) {
        releaseGlObjects();

        vertices_ = std::move(other.vertices_);
        indices_ = std::move(other.indices_);
        textures_ = std::move(other.textures_);

        vao_ = std::exchange(other.vao_, 0);
        vbo_ = std::exchange(other.vbo_, 0);
        ebo_ = std::exchange(other.ebo_, 0);
    }

    return *this;
}

void Mesh::draw(const Shader& shader) const
{
    bool hasDiffuseTexture = false;
    unsigned int diffuseIndex = 1;

    for (unsigned int i = 0; i < textures_.size(); ++i) {
        if (textures_[i].id == 0) continue;
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures_[i].id);
        if (textures_[i].type == "texture_diffuse") {
            shader.setInt("texture_diffuse" + std::to_string(diffuseIndex), static_cast<int>(i));
            ++diffuseIndex;
            hasDiffuseTexture = true;
        }
    }
    shader.setBool("uHasTexture", hasDiffuseTexture);
    drawDirect();
    glActiveTexture(GL_TEXTURE0);
}

void Mesh::drawDirect() const
{
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::setupMesh()
{
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices_.size() * sizeof(unsigned int)), indices_.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texCoords)));

    glBindVertexArray(0);
}

void Mesh::releaseGlObjects()
{
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
}

void Mesh::attachTextureIfMissing(const TextureAsset& texture)
{
    for (const TextureAsset& existing : textures_) {
        if (existing.type == "texture_diffuse") {
            return;
        }
    }
    textures_.push_back(texture);
}
