#include "core/Application.h"
#include "render/Renderer.h"
#include "render/Shader.h"
#include "scene/Scene.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string resolvePath(const std::vector<std::string>& candidates)
{
    const std::filesystem::path current = std::filesystem::current_path();
    const std::vector<std::filesystem::path> roots{current, current / "..", current / "../.."};

    for (const auto& candidate : candidates) {
        std::filesystem::path p(candidate);
        if (p.is_absolute() && std::filesystem::exists(p))
            return std::filesystem::weakly_canonical(p).string();
    }
    for (const auto& root : roots) {
        for (const auto& candidate : candidates) {
            auto full = root / candidate;
            if (std::filesystem::exists(full))
                return std::filesystem::weakly_canonical(full).string();
        }
    }
    return {};
}

} // namespace

int main()
{
    AppConfig appCfg;
    appCfg.width = 1280; appCfg.height = 720;
    appCfg.title = "Classroom Renderer";
    appCfg.cameraPos = glm::vec3(0.0f, 2.0f, 8.0f);
    appCfg.cameraFov = 45.0f; appCfg.cameraSpeed = 5.0f; appCfg.cameraSensitivity = 0.15f;
    Application app(appCfg);
    if (app.window() == nullptr) {
        return -1;
    }

    const std::string vertPath = resolvePath({"shaders/model.vert"});
    const std::string fragPath = resolvePath({"shaders/model.frag"});
    if (vertPath.empty() || fragPath.empty()) {
        std::cerr << "Shader files not found.\n"; return -1;
    }
    Shader shader(vertPath.c_str(), fragPath.c_str());
    if (!shader.isValid()) {
        std::cerr << "Shader compilation failed.\n"; return -1;
    }

    auto scene = std::make_unique<Scene>();

    const std::string fallbackTex = resolvePath({
        "resources/textures/Wood066_1K-PNG_Color.png",
        "resources/textures/blenderkit_logo.png",
    });

    const std::string modelPath = resolvePath({
        "resources/models/Tsukinomori.obj",
    });

    // ---- Add models here ----
    if (!modelPath.empty())
        scene->addModel({modelPath, Transform{glm::vec3(0.0f, 0.0f, 0.0f)}, "Model"});

    // const std::string deskPath = resolvePath({"resources/models/classroom_desk.glb"});
    // if (!deskPath.empty())
    //     scene.addModel({deskPath, Transform{glm::vec3(5.0f, 0.0f, 0.0f)}, "Desk"});

    if (scene->entries().empty()) {
        std::cerr << "No models in scene.\n"; return -1;
    }

    Renderer renderer(shader, app.camera());
    renderer.setLightDirection(glm::vec3(-0.8f, -1.0f, -0.3f));
    renderer.setFallbackColor(glm::vec3(0.8f, 0.8f, 0.8f));

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* loaderWindow = glfwCreateWindow(1, 1, "Asset Loader", nullptr, app.window());
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    if (loaderWindow == nullptr) {
        std::cerr << "[Main] Failed to create shared OpenGL context for background loading.\n";
        return -1;
    }

    auto loadingScene = std::move(scene);
    std::unique_ptr<Scene> loadedScene;
    std::mutex loadedSceneMutex;
    std::atomic_bool loadDone = false;
    std::atomic_bool loadFailed = false;

    std::thread loader([loaderWindow, fallbackTex, sceneToLoad = std::move(loadingScene), &loadedScene, &loadedSceneMutex, &loadDone, &loadFailed]() mutable {
        glfwMakeContextCurrent(loaderWindow);
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            std::cerr << "[Main] Failed to initialize GLAD in loader context.\n";
            loadFailed.store(true, std::memory_order_release);
            loadDone.store(true, std::memory_order_release);
            return;
        }

        std::cout << "[Main] Loading " << sceneToLoad->entries().size() << " model(s) in background...\n";
        sceneToLoad->loadAll(fallbackTex);
        sceneToLoad->releaseVertexArraysForCurrentContext();

        // Flush all pending GL commands so textures are visible to the main context
        glFinish();

        glfwMakeContextCurrent(nullptr);

        {
            std::lock_guard<std::mutex> lock(loadedSceneMutex);
            loadedScene = std::move(sceneToLoad);
            loadFailed.store(loadedScene->modelCount() == 0, std::memory_order_release);
        }
        loadDone.store(true, std::memory_order_release);
    });

    Scene* activeScene = nullptr;
    bool sceneActivated = false;
    bool lWasDown = false;
    bool nWasDown = false;

    app.run([&](float /*dt*/) {
        // ---- L key: toggle light ----
        bool lNow = (glfwGetKey(app.window(), GLFW_KEY_L) == GLFW_PRESS);
        if (lNow && !lWasDown) {
            renderer.toggleLight();
            std::cout << "[Main] Light " << (renderer.isLightOn() ? "ON" : "OFF") << "\n";
        }
        lWasDown = lNow;

        // ---- N key: toggle day/night ----
        bool nNow = (glfwGetKey(app.window(), GLFW_KEY_N) == GLFW_PRESS);
        if (nNow && !nWasDown) {
            renderer.toggleDayNight();
        }
        nWasDown = nNow;

        if (loadDone.load(std::memory_order_acquire) && !sceneActivated) {
            std::lock_guard<std::mutex> lock(loadedSceneMutex);
            if (!loadFailed.load(std::memory_order_acquire) && loadedScene) {
                loadedScene->createVertexArraysForCurrentContext();
                activeScene = loadedScene.get();
                std::cout << "[Main] Background model loading complete.\n";
            } else {
                std::cerr << "[Main] Background model loading failed.\n";
            }
            sceneActivated = true;
        }

        if (activeScene != nullptr) {
            renderer.render(*activeScene);
        }
    });

    if (loader.joinable()) {
        loader.join();
    }
    glfwMakeContextCurrent(app.window());
    loadedScene.reset();
    glfwDestroyWindow(loaderWindow);

    return 0;
}