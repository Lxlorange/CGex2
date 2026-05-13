#include "scene/Model.h"
#include "render/Texture.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <iostream>
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

} // namespace

Model::Model(const std::string& modelPath, const std::string& fallbackDiffuseTexturePath)
{
    loadModel(modelPath);
    if (loaded_) {
        applyFallbackDiffuseTexture(fallbackDiffuseTexturePath);
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

void Model::draw(const Shader& shader) const
{
    struct Group { GLuint texId; std::vector<const Mesh*> meshes; };
    std::vector<Group> groups;

    for (const Mesh& mesh : meshes_) {
        GLuint tid = 0;
        for (const auto& t : mesh.textures()) {
            if (t.type == "texture_diffuse" && t.id != 0) { tid = t.id; break; }
        }
        if (groups.empty() || groups.back().texId != tid)
            groups.push_back({tid, {}});
        groups.back().meshes.push_back(&mesh);
    }

    for (const auto& g : groups) {
        if (g.texId != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g.texId);
            shader.setInt("texture_diffuse1", 0);
            shader.setBool("uHasTexture", true);
        } else {
            shader.setBool("uHasTexture", false);
        }
        for (const Mesh* m : g.meshes) m->drawDirect();
    }
    glActiveTexture(GL_TEXTURE0);
}

void Model::loadModel(const std::string& modelPath)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        modelPath,
        aiProcess_Triangulate
            | aiProcess_GenSmoothNormals
            | aiProcess_JoinIdenticalVertices
            | aiProcess_FlipUVs
            | aiProcess_ImproveCacheLocality);

    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || scene->mRootNode == nullptr) {
        std::cerr << "[Model] Failed to load model with Assimp.\n"
                  << "  Path: " << modelPath << '\n'
                  << "  Assimp error: " << importer.GetErrorString() << '\n';
        loaded_ = false;
        return;
    }

    directory_ = std::filesystem::path(modelPath).parent_path().string();
    processNode(scene->mRootNode, scene);
    loaded_ = !meshes_.empty();

    if (!loaded_) {
        std::cerr << "[Model] Model loaded but no mesh data was extracted.\n"
                  << "  Path: " << modelPath << '\n';
    } else {
        std::cerr << "[Model] Loaded model successfully.\n"
                  << "  Path: " << modelPath << '\n'
                  << "  Mesh count: " << meshes_.size() << '\n';
    }
}

void Model::processNode(aiNode* node, const aiScene* scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes_.push_back(processMesh(mesh, scene));
        if (meshes_.size() % 50 == 0) glfwPollEvents();
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

    vertices.reserve(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex vertex;

        vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        if (mesh->HasNormals()) {
            vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
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

    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        std::vector<TextureAsset> diffuseTextures = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
        textures.insert(textures.end(), diffuseTextures.begin(), diffuseTextures.end());
    }

    return Mesh(std::move(vertices), std::move(indices), std::move(textures));
}

std::vector<TextureAsset> Model::loadMaterialTextures(aiMaterial* material, aiTextureType type, const std::string& typeName, const aiScene* scene)
{
    std::vector<TextureAsset> textures;

    const unsigned int count = material->GetTextureCount(type);
    for (unsigned int i = 0; i < count; ++i) {
        aiString aiPath;
        material->GetTexture(type, i, &aiPath);
        std::string rawPath = aiPath.C_Str();

        // ---- Embedded texture (*0, *1, ...) from glTF/GLB ----
        if (rawPath.size() >= 2 && rawPath[0] == '*' && rawPath[1] >= '0' && rawPath[1] <= '9' && scene) {
            int idx = std::stoi(rawPath.substr(1));
            if (idx >= 0 && idx < static_cast<int>(scene->mNumTextures) && scene->mTextures[idx]) {
                aiTexture* tex = scene->mTextures[idx];
                GLuint glId = 0;

                if (tex->mHeight == 0) {
                    // Compressed (PNG/JPG) — raw bytes in pcData
                    glId = loadTexture2DFromMemory(
                        reinterpret_cast<unsigned char*>(tex->pcData), tex->mWidth, true);
                } else {
                    // Uncompressed ARGB8888
                    glId = loadTexture2DFromMemory(
                        reinterpret_cast<unsigned char*>(tex->pcData), tex->mWidth * tex->mHeight * 4, true);
                }

                if (glId != 0) {
                    TextureAsset ta{glId, typeName, rawPath};
                    textures.push_back(ta);
                    loadedTextures_.push_back(ta);
                } else {
                    std::cerr << "[Model] Embedded texture decode failed: *" << idx << "\n";
                }
                continue;
            }
        }

        // ---- File-based texture ----
        std::filesystem::path resolvedPath(rawPath);
        if (!resolvedPath.is_absolute())
            resolvedPath = std::filesystem::path(directory_) / resolvedPath;

        const std::string normalized = normalizePathString(resolvedPath);
        bool alreadyLoaded = false;
        for (const auto& lt : loadedTextures_) {
            if (lt.path == normalized) { textures.push_back(lt); alreadyLoaded = true; break; }
        }
        if (alreadyLoaded) continue;

        TextureAsset ta;
        ta.id = loadTexture2DFromFile(normalized);
        ta.type = typeName;
        ta.path = normalized;
        if (ta.id != 0) {
            textures.push_back(ta);
            loadedTextures_.push_back(ta);
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
