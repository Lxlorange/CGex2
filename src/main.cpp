#include "core/Application.h"
#include "render/Renderer.h"
#include "render/Shader.h"
#include "scene/Scene.h"

#include <filesystem>
#include <iostream>
#include <string>
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
    Application::Config appCfg;
    appCfg.width = 1280; appCfg.height = 720;
    appCfg.title = "Classroom Renderer";
    appCfg.cameraPos = glm::vec3(0.0f, 2.0f, 8.0f);
    appCfg.cameraFov = 45.0f; appCfg.cameraSpeed = 5.0f; appCfg.cameraSensitivity = 0.15f;
    Application app(appCfg);

    const std::string vertPath = resolvePath({"shaders/model.vert"});
    const std::string fragPath = resolvePath({"shaders/model.frag"});
    if (vertPath.empty() || fragPath.empty()) {
        std::cerr << "Shader files not found.\n"; return -1;
    }
    Shader shader(vertPath.c_str(), fragPath.c_str());
    if (!shader.isValid()) {
        std::cerr << "Shader compilation failed.\n"; return -1;
    }

    Scene scene;

    const std::string fallbackTex = resolvePath({
        "resources/textures/Wood066_1K-PNG_Color.png",
        "resources/textures/blenderkit_logo.png",
    });

    const std::string modelPath = resolvePath({
        "resources/models/Tsukinomori.obj",
    });

    // ---- Add models here ----
    if (!modelPath.empty())
        scene.addModel({modelPath, Transform{glm::vec3(0.0f, 0.0f, 0.0f)}, "Model"});

    // const std::string deskPath = resolvePath({"resources/models/classroom_desk.glb"});
    // if (!deskPath.empty())
    //     scene.addModel({deskPath, Transform{glm::vec3(5.0f, 0.0f, 0.0f)}, "Desk"});

    if (scene.entries().empty()) {
        std::cerr << "No models in scene.\n"; return -1;
    }
    std::cout << "[Main] Loading " << scene.entries().size() << " model(s)...\n";
    scene.loadAll(fallbackTex);

    Renderer renderer(shader, app.camera());
    renderer.setLightDirection(glm::vec3(-0.8f, -1.0f, -0.3f));
    renderer.setFallbackColor(glm::vec3(0.8f, 0.8f, 0.8f));

    app.run([&](float /*dt*/) { renderer.render(scene); });
    return 0;
}
