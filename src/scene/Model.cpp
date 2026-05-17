#include "scene/Model.h"
#include "render/Texture.h"

#include <assimp/Importer.hpp>
#include <assimp/ProgressHandler.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace {

std::string normalizePathString(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonicalPath.generic_string();
    }
    return path.lexically_normal().generic_string();
}

std::string extractTexturePathToken(const std::string& rawPath)
{
    std::istringstream stream(rawPath);
    std::string token;
    std::string lastToken;
    while (stream >> token) {
        lastToken = token;
    }
    return lastToken.empty() ? rawPath : lastToken;
}

std::string makeEmbeddedTextureKey(const std::string& path)
{
    return "embedded:" + path;
}

std::string makeFileTextureKey(const std::string& normalizedPath)
{
    return "file:" + normalizedPath;
}

unsigned countMeshesUnderNode(const aiNode* node)
{
    unsigned count = node->mNumMeshes;
    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        count += countMeshesUnderNode(node->mChildren[i]);
    }
    return count;
}

class AssimpFileProgress final : public Assimp::ProgressHandler {
public:
    explicit AssimpFileProgress(Model::LoadProgressCallback cb)
        : cb_(std::move(cb))
    {
    }

    bool Update(float percentage) override
    {
        if (!cb_) {
            return true;
        }
        if (percentage < 0.0f) {
            cb_(0.12f, "Assimp: processing...");
            return true;
        }
        const float p = std::clamp(percentage, 0.0f, 1.0f);
        cb_(0.05f + 0.28f * p, "Assimp: reading / parsing file...");
        return true;
    }

private:
    Model::LoadProgressCallback cb_;
};

} // namespace

Model::Model(const std::string& modelPath, const std::string& fallbackDiffuseTexturePath,
             LoadProgressCallback onProgress)
    : progressCb_(std::move(onProgress))
{
    loadModel(modelPath);
    (void)fallbackDiffuseTexturePath;
    if (loaded_) {
        emitProgress(1.0f, "Model ready.");
    }
}

Model::~Model()
{
    std::unordered_set<GLuint> deleted;
    for (const TextureAsset& texture : loadedTextures_) {
        if (texture.id != 0 && deleted.insert(texture.id).second) {
            glDeleteTextures(1, &texture.id);
        }
    }
}

void Model::emitProgress(float normalized, const char* status)
{
    if (!progressCb_) {
        return;
    }
    progressCb_(std::clamp(normalized, 0.0f, 1.0f), status ? status : "");
}

void Model::draw(const Shader& shader) const
{
    for (const Mesh& mesh : meshes_) {
        mesh.draw(shader);
    }
}

void Model::createVertexArraysForCurrentContext()
{
    for (Mesh& mesh : meshes_) {
        mesh.createVertexArrayForCurrentContext();
    }
}

void Model::releaseVertexArraysForCurrentContext()
{
    for (Mesh& mesh : meshes_) {
        mesh.releaseVertexArrayForCurrentContext();
    }
}

void Model::loadModel(const std::string& modelPath)
{
    emitProgress(0.0f, "Starting model load...");

    Assimp::Importer importer;
    AssimpFileProgress assimpProgress(progressCb_);
    if (progressCb_) {
        importer.SetProgressHandler(&assimpProgress);
    }

    emitProgress(0.02f, "Reading model file (Assimp)...");
    const aiScene* scene = importer.ReadFile(
        modelPath,
        aiProcess_Triangulate
            | aiProcess_CalcTangentSpace
            | aiProcess_GenSmoothNormals
            | aiProcess_JoinIdenticalVertices
            | aiProcess_ImproveCacheLocality);

    importer.SetProgressHandler(nullptr);

    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || scene->mRootNode == nullptr) {
        std::cerr << "[Model] Failed to load model with Assimp.\n"
                  << "  Path: " << modelPath << '\n'
                  << "  Assimp error: " << importer.GetErrorString() << '\n';
        loaded_ = false;
        emitProgress(0.0f, "Load failed.");
        return;
    }

    directory_ = std::filesystem::path(modelPath).parent_path().string();
    meshDone_ = 0;
    meshTotal_ = std::max(1u, countMeshesUnderNode(scene->mRootNode));
    emitProgress(0.36f, "Building GPU meshes...");

    processNode(scene->mRootNode, scene);
    loaded_ = !meshes_.empty();

    if (!loaded_) {
        std::cerr << "[Model] Model loaded but no mesh data was extracted.\n"
                  << "  Path: " << modelPath << '\n';
        emitProgress(0.0f, "No mesh data extracted.");
    } else {
        std::cerr << "[Model] Loaded model successfully.\n"
                  << "  Path: " << modelPath << '\n'
                  << "  Mesh count: " << meshes_.size() << '\n';
        emitProgress(0.85f, "Geometry upload complete.");
    }
}

void Model::processNode(aiNode* node, const aiScene* scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes_.push_back(processMesh(mesh, scene));

        ++meshDone_;
        if (progressCb_) {
            const float unit = static_cast<float>(meshDone_) / static_cast<float>(meshTotal_);
            const float progress = std::min(0.84f, 0.36f + 0.48f * unit);
            if (meshDone_ == 1u || (meshDone_ % 2u) == 0u || meshDone_ == meshTotal_) {
                emitProgress(progress, "Uploading mesh data to GPU...");
            }
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<TextureAsset> textures;
    Material materialData;

    vertices.reserve(mesh->mNumVertices);
    indices.reserve(mesh->mNumFaces * 3);
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex vertex;

        vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        if (mesh->HasNormals()) {
            vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (mesh->HasTangentsAndBitangents()) {
            vertex.tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
            vertex.bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
        } else {
            vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            vertex.bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        if (mesh->mTextureCoords[0] != nullptr) {
            vertex.texCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        } else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    if (!vertices.empty()) {
        glm::vec3 meshMin = vertices[0].position;
        glm::vec3 meshMax = vertices[0].position;
        for (std::size_t vi = 1; vi < vertices.size(); ++vi) {
            meshMin = glm::min(meshMin, vertices[vi].position);
            meshMax = glm::max(meshMax, vertices[vi].position);
        }
        if (!localAabbValid_) {
            localAabbMin_ = meshMin;
            localAabbMax_ = meshMax;
            localAabbValid_ = true;
        } else {
            localAabbMin_ = glm::min(localAabbMin_, meshMin);
            localAabbMax_ = glm::max(localAabbMax_, meshMax);
        }
    }

    if (mesh->mMaterialIndex < scene->mNumMaterials && scene->mMaterials[mesh->mMaterialIndex] != nullptr) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        aiColor3D diffuseColor(0.8f, 0.8f, 0.8f);
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
            materialData.diffuse = glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
        }

        auto diffuseTextures = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
        textures.insert(textures.end(), diffuseTextures.begin(), diffuseTextures.end());
        auto specularTextures = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", scene);
        textures.insert(textures.end(), specularTextures.begin(), specularTextures.end());
        auto shininessTextures = loadMaterialTextures(material, aiTextureType_SHININESS, "texture_roughness", scene);
        textures.insert(textures.end(), shininessTextures.begin(), shininessTextures.end());
        auto normalTextures = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", scene);
        textures.insert(textures.end(), normalTextures.begin(), normalTextures.end());
        auto heightTextures = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", scene);
        textures.insert(textures.end(), heightTextures.begin(), heightTextures.end());
    }

    return Mesh(std::move(vertices), std::move(indices), std::move(textures), materialData);
}

std::vector<TextureAsset> Model::loadMaterialTextures(aiMaterial* material, aiTextureType type, const std::string& typeName, const aiScene* scene)
{
    std::vector<TextureAsset> textures;

    const unsigned int count = material->GetTextureCount(type);
    for (unsigned int i = 0; i < count; ++i) {
        aiString aiPath;
        if (material->GetTexture(type, i, &aiPath) != AI_SUCCESS) {
            continue;
        }

        std::string rawPath = extractTexturePathToken(aiPath.C_Str());

        const aiTexture* embeddedTexture = scene ? scene->GetEmbeddedTexture(rawPath.c_str()) : nullptr;
        if (embeddedTexture != nullptr) {
            const std::string cacheKey = makeEmbeddedTextureKey(rawPath);
            auto cached = textureCache_.find(cacheKey);
            if (cached != textureCache_.end()) {
                textures.push_back(TextureAsset{cached->second, typeName, cacheKey});
                continue;
            }

            GLuint glId = 0;
            if (embeddedTexture->mHeight == 0) {
                glId = loadTexture2DFromMemory(
                    reinterpret_cast<const unsigned char*>(embeddedTexture->pcData),
                    static_cast<int>(embeddedTexture->mWidth),
                    true);
            } else {
                glId = createTexture2DFromRGBAPixels(
                    embeddedTexture->pcData,
                    static_cast<int>(embeddedTexture->mWidth),
                    static_cast<int>(embeddedTexture->mHeight),
                    true);
            }

            if (glId != 0) {
                textureCache_[cacheKey] = glId;
                TextureAsset texture{glId, typeName, cacheKey};
                textures.push_back(texture);
                loadedTextures_.push_back(texture);
            } else {
                std::cerr << "[Model] Embedded texture decode failed: " << rawPath << "\n";
            }
            continue;
        }

        if (rawPath.size() >= 2 && rawPath[0] == '*' && rawPath[1] >= '0' && rawPath[1] <= '9' && scene) {
            const int idx = std::stoi(rawPath.substr(1));
            if (idx >= 0 && idx < static_cast<int>(scene->mNumTextures) && scene->mTextures[idx] != nullptr) {
                const std::string cacheKey = makeEmbeddedTextureKey(rawPath);
                auto cached = textureCache_.find(cacheKey);
                if (cached != textureCache_.end()) {
                    textures.push_back(TextureAsset{cached->second, typeName, cacheKey});
                    continue;
                }

                const aiTexture* tex = scene->mTextures[idx];
                GLuint glId = 0;
                if (tex->mHeight == 0) {
                    glId = loadTexture2DFromMemory(
                        reinterpret_cast<const unsigned char*>(tex->pcData),
                        static_cast<int>(tex->mWidth),
                        true);
                } else {
                    glId = createTexture2DFromRGBAPixels(
                        tex->pcData,
                        static_cast<int>(tex->mWidth),
                        static_cast<int>(tex->mHeight),
                        true);
                }

                if (glId != 0) {
                    textureCache_[cacheKey] = glId;
                    TextureAsset texture{glId, typeName, cacheKey};
                    textures.push_back(texture);
                    loadedTextures_.push_back(texture);
                } else {
                    std::cerr << "[Model] Embedded texture decode failed: " << rawPath << "\n";
                }
                continue;
            }
        }

        std::filesystem::path resolvedPath(rawPath);
        if (!resolvedPath.is_absolute()) {
            resolvedPath = std::filesystem::path(directory_) / resolvedPath;
        }

        const std::string normalized = normalizePathString(resolvedPath);
        const std::string cacheKey = makeFileTextureKey(normalized);

        auto cached = textureCache_.find(cacheKey);
        if (cached != textureCache_.end()) {
            textures.push_back(TextureAsset{cached->second, typeName, normalized});
            continue;
        }

        TextureAsset texture;
        texture.id = loadTexture2DFromFile(normalized);
        texture.type = typeName;
        texture.path = normalized;
        if (texture.id != 0) {
            textureCache_[cacheKey] = texture.id;
            textures.push_back(texture);
            loadedTextures_.push_back(texture);
        } else {
            std::cerr << "[Model] Texture load failed: " << normalized << "\n";
        }
    }
    return textures;
}

void Model::applyFallbackDiffuseTexture(const std::string& fallbackDiffuseTexturePath)
{
    if (fallbackDiffuseTexturePath.empty()) {
        return;
    }

    std::filesystem::path fallbackPath(fallbackDiffuseTexturePath);
    if (!fallbackPath.is_absolute()) {
        fallbackPath = std::filesystem::current_path() / fallbackPath;
    }

    const std::string normalizedFallback = normalizePathString(fallbackPath);
    const GLuint textureId = loadTexture2DFromFile(normalizedFallback);
    if (textureId == 0) {
        std::cerr << "[Model] Fallback texture could not be loaded.\n"
                  << "  Path: " << normalizedFallback << '\n';
        return;
    }

    const TextureAsset fallbackTexture {textureId, "texture_diffuse", normalizedFallback};
    for (Mesh& mesh : meshes_) {
        mesh.attachTextureIfMissing(fallbackTexture);
    }

    loadedTextures_.push_back(fallbackTexture);
}
