#include "scene/Scene.h"
#include "render/Shader.h"
#include <iostream>

void Scene::loadAll(const std::string& fallbackTex)
{
    for (const auto& entry : entries_) {
        if (modelCache_.count(entry.path)) continue;
        std::cout << "[Scene] Loading " << entry.name << "...\n";
        auto m = std::make_unique<Model>(entry.path, fallbackTex);
        if (!m->isLoaded()) {
            std::cerr << "[Scene] Failed: " << entry.name << " (" << entry.path << ")\n";
            continue;
        }
        modelCache_[entry.path] = std::move(m);
    }
}

void Scene::drawAll(Shader& shader) const
{
    for (const auto& entry : entries_) {
        auto it = modelCache_.find(entry.path);
        if (it == modelCache_.end() || !it->second->isLoaded()) continue;
        shader.setMat4("uModel", entry.transform.matrix());
        it->second->draw(shader);
    }
}

void Scene::createVertexArraysForCurrentContext()
{
    for (auto& [path, model] : modelCache_) {
        (void)path;
        if (model) {
            model->createVertexArraysForCurrentContext();
        }
    }
}

void Scene::releaseVertexArraysForCurrentContext()
{
    for (auto& [path, model] : modelCache_) {
        (void)path;
        if (model) {
            model->releaseVertexArraysForCurrentContext();
        }
    }
}
