#pragma once

#include "render/Mesh.h"

#include <assimp/scene.h>

#include <string>
#include <unordered_map>
#include <vector>

class Model {
public:
    explicit Model(const std::string& modelPath, const std::string& fallbackDiffuseTexturePath = {});
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(Model&&) = delete;

    bool isLoaded() const noexcept { return loaded_; }
    void draw(const Shader& shader) const;
    void createVertexArraysForCurrentContext();
    void releaseVertexArraysForCurrentContext();

private:
    std::vector<Mesh> meshes_;
    std::vector<TextureAsset> loadedTextures_;
    std::unordered_map<std::string, GLuint> textureCache_;
    std::string directory_;
    bool loaded_ = false;

    void loadModel(const std::string& modelPath);
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<TextureAsset> loadMaterialTextures(aiMaterial* material, aiTextureType type, const std::string& typeName, const aiScene* scene);
    void applyFallbackDiffuseTexture(const std::string& fallbackDiffuseTexturePath);
};
