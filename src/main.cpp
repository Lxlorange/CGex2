#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "collision/AABB.h"
#include "collision/CollisionPrimitives.h"
#include "collision/DebugAabbDrawer.h"
#include "core/Application.h"
#include "core/Camera.h"
#include "render/BloomRenderer.h"
#include "render/HDRFramebuffer.h"
#include "render/LightManager.h"
#include "render/Renderer.h"
#include "render/Shader.h"
#include "render/SSAORenderer.h"
#include "scene/Scene.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

struct LightingState {
    bool isDay = true;
    bool pointLightsOn = true;
    bool key1WasDown = false;
    bool key2WasDown = false;
    bool keyOWasDown = false;
    bool keyCWasDown = false;
};

void restoreGlStateForScene(GLFWwindow* window)
{
    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    if (w > 0 && h > 0) {
        glViewport(0, 0, w, h);
    }

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    for (int i = 0; i < 8; ++i) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + i));
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

std::string resolvePath(const std::vector<std::string>& candidates)
{
    const std::filesystem::path current = std::filesystem::current_path();
    const std::vector<std::filesystem::path> roots{current, current / "..", current / "../.."};

    for (const auto& candidate : candidates) {
        std::filesystem::path path(candidate);
        if (path.is_absolute() && std::filesystem::exists(path)) {
            return std::filesystem::weakly_canonical(path).string();
        }
    }

    for (const auto& root : roots) {
        for (const auto& candidate : candidates) {
            const auto full = root / candidate;
            if (std::filesystem::exists(full)) {
                return std::filesystem::weakly_canonical(full).string();
            }
        }
    }
    return {};
}

namespace {

// Eye position is Camera::Position; capsule is the walking body below it.
constexpr float kCameraCapsuleRadius = 0.22f;
constexpr float kCameraCapsuleTopOffset = -0.20f;
constexpr float kCameraCapsuleBottomOffset = -1.32f;
constexpr float kCollisionEpsilon = 0.002f;
constexpr int kCollisionIterations = 4;

Capsule cameraBodyCapsule(const glm::vec3& eyePosition)
{
    return Capsule(
        eyePosition + glm::vec3(0.0f, kCameraCapsuleBottomOffset + kCameraCapsuleRadius, 0.0f),
        eyePosition + glm::vec3(0.0f, kCameraCapsuleTopOffset - kCameraCapsuleRadius, 0.0f),
        kCameraCapsuleRadius);
}

Contact strongestCapsuleContact(const Capsule& capsule, const std::vector<NamedAABB>& colliders)
{
    Contact strongest;
    for (const NamedAABB& collider : colliders) {
        Contact contact = capsuleAABBContact(capsule, collider.box);
        if (contact.hit && contact.penetration > strongest.penetration) {
            strongest = contact;
        }
    }
    return strongest;
}

Contact strongestCapsuleContact(const Capsule& capsule, const std::vector<NamedOBB>& colliders)
{
    Contact strongest;
    for (const NamedOBB& collider : colliders) {
        Contact contact = capsuleOBBContact(capsule, collider.box);
        if (contact.hit && contact.penetration > strongest.penetration) {
            strongest = contact;
        }
    }
    return strongest;
}

bool capsuleIntersectsAny(const Capsule& capsule, const std::vector<NamedAABB>& colliders)
{
    return strongestCapsuleContact(capsule, colliders).hit;
}

bool capsuleIntersectsAny(const Capsule& capsule, const std::vector<NamedOBB>& colliders)
{
    return strongestCapsuleContact(capsule, colliders).hit;
}

#if 0
void resolveCameraCollision(Camera& camera, const glm::vec3& previousPosition,
                            const std::vector<NamedOBB>& colliders)
{
    if (colliders.empty()) {
        return;
    }

    const glm::vec3 desired = camera.Position;
    if (!intersectsAny(cameraBodyAabb(desired), colliders)) {
        return;
    }

    glm::vec3 candidate = previousPosition;

    auto tryAxis = [&](int axis) {
        glm::vec3 next = candidate;
        next[axis] = desired[axis];
        if (!intersectsAny(cameraBodyAabb(next), colliders)) {
            candidate = next;
        }
    };

    // Horizontal first — reduces corner snagging when strafing along walls.
    tryAxis(0);
    tryAxis(2);
    tryAxis(1);

    candidate = depenetratePosition(candidate, colliders);

    if (intersectsAny(cameraBodyAabb(candidate), colliders)) {
        candidate = depenetratePosition(previousPosition, colliders);
        if (intersectsAny(cameraBodyAabb(candidate), colliders)) {
            candidate = previousPosition;
        }
    }

    camera.Position = candidate;
}
#endif

void resolveCameraCollision(Camera& camera, const glm::vec3& previousPosition,
                            const std::vector<NamedOBB>& colliders)
{
    if (colliders.empty()) {
        return;
    }

    glm::vec3 current = previousPosition;
    glm::vec3 remaining = camera.Position - previousPosition;
    if (glm::dot(remaining, remaining) <= 1e-12f) {
        return;
    }

    for (int iter = 0; iter < kCollisionIterations; ++iter) {
        if (glm::dot(remaining, remaining) <= 1e-10f) {
            break;
        }

        glm::vec3 candidate = current + remaining;
        Contact contact = strongestCapsuleContact(cameraBodyCapsule(candidate), colliders);
        if (!contact.hit) {
            current = candidate;
            remaining = glm::vec3(0.0f);
            break;
        }

        current = candidate + contact.normal * (contact.penetration + kCollisionEpsilon);
        remaining = slideVector(remaining, contact.normal);

        const float normalMotion = glm::dot(remaining, contact.normal);
        if (normalMotion < 0.0f) {
            remaining -= contact.normal * normalMotion;
        }
    }

    for (int iter = 0; iter < kCollisionIterations; ++iter) {
        Contact contact = strongestCapsuleContact(cameraBodyCapsule(current), colliders);
        if (!contact.hit) {
            break;
        }
        current += contact.normal * (contact.penetration + kCollisionEpsilon);
    }

    if (capsuleIntersectsAny(cameraBodyCapsule(current), colliders)) {
        current = previousPosition;
    }
    camera.Position = current;
}

bool isUsefulCameraCollider(const NamedOBB& collider, const glm::vec3& sceneExtent)
{
    const glm::vec3 full = collider.box.halfExtents * 2.0f;
    if (full.x <= 0.002f || full.y <= 0.002f || full.z <= 0.002f) {
        return false;
    }

    const float volume = full.x * full.y * full.z;
    const float sceneVolume = std::max(sceneExtent.x * sceneExtent.y * sceneExtent.z, 0.001f);
    if (volume > sceneVolume * 0.35f) {
        return false;
    }

    const int spanningAxes =
        (full.x > sceneExtent.x * 0.72f ? 1 : 0)
        + (full.y > sceneExtent.y * 0.72f ? 1 : 0)
        + (full.z > sceneExtent.z * 0.72f ? 1 : 0);
    return spanningAxes < 2;
}

} // namespace

void handleLightingHotkeys(GLFWwindow* window, LightingState& lighting)
{
    const bool key1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    const bool key2 = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    const bool keyO = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    const bool keyC = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;

    if (key1 && !lighting.key1WasDown) {
        lighting.isDay = true;
    }
    if (key2 && !lighting.key2WasDown) {
        lighting.isDay = false;
    }

    if (keyO && !lighting.keyOWasDown) {
        lighting.pointLightsOn = true;
    }
    if (keyC && !lighting.keyCWasDown) {
        lighting.pointLightsOn = false;
    }

    lighting.key1WasDown = key1;
    lighting.key2WasDown = key2;
    lighting.keyOWasDown = keyO;
    lighting.keyCWasDown = keyC;
}

void syncLightingUniforms(Shader& shader, Renderer& renderer, const Camera& camera,
                          const LightingState& lighting, const LightManager& lightManager)
{
    shader.use();
    if (lighting.isDay) {
        renderer.setDirectionalShadowsEnabled(true);
        shader.setVec3("dirLight.ambient", glm::vec3(0.10f, 0.105f, 0.12f));
        shader.setVec3("dirLight.diffuse", glm::vec3(0.88f, 0.82f, 0.68f));
        shader.setVec3("dirLight.specular", glm::vec3(0.22f, 0.20f, 0.18f));
        shader.setInt("uPointLightsOn", lighting.pointLightsOn ? 1 : 0);
    } else {
        renderer.setDirectionalShadowsEnabled(false);
        shader.setVec3("dirLight.ambient", glm::vec3(0.02f, 0.02f, 0.05f));
        shader.setVec3("dirLight.diffuse", glm::vec3(0.05f, 0.05f, 0.10f));
        shader.setVec3("dirLight.specular", glm::vec3(0.10f, 0.10f, 0.10f));
        shader.setInt("uPointLightsOn", lighting.pointLightsOn ? 1 : 0);
    }

    const float lampEmissionScale = lighting.pointLightsOn ? lightManager.pointLightStrength : 0.0f;
    shader.setFloat("uLampEmissionScale", lampEmissionScale);

    shader.setVec3("spotLight.position", camera.Position);
    shader.setVec3("spotLight.direction", camera.Front);
    shader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
    shader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(17.5f)));
    shader.setFloat("spotLight.constant", 1.0f);
    shader.setFloat("spotLight.linear", 0.045f);
    shader.setFloat("spotLight.quadratic", 0.015f);
    shader.setVec3("spotLight.ambient", glm::vec3(0.0f));
    if (lighting.isDay) {
        shader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
        shader.setVec3("spotLight.specular", glm::vec3(0.0f));
    } else {
        shader.setVec3("spotLight.diffuse", glm::vec3(0.78f, 0.76f, 0.82f) * lightManager.spotLightStrength);
        shader.setVec3("spotLight.specular", glm::vec3(0.58f, 0.56f, 0.60f) * lightManager.spotLightStrength);
    }
}

void drawSunMarker(DebugAabbDrawer& drawer, GLFWwindow* window, const Camera& camera, const glm::vec3& direction)
{
    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    if (h <= 0) {
        h = 1;
    }

    const glm::vec3 sunPos = camera.Position - glm::normalize(direction) * 12.0f + glm::vec3(0.0f, 3.0f, 0.0f);
    std::vector<NamedAABB> marker;
    marker.push_back({"Sun", AABB::fromCenterHalfExtents(sunPos, glm::vec3(0.22f))});

    const glm::mat4 proj = glm::perspective(
        glm::radians(camera.Zoom),
        static_cast<float>(w) / static_cast<float>(h), 0.1f, 300.0f);
    drawer.draw(proj, camera.GetViewMatrix(), marker, glm::vec3(1.0f, 0.82f, 0.12f));
}

void alignPointLightsToEmissiveMeshes(LightManager& lightManager, const Scene& scene)
{
    std::vector<EmissiveMeshInfo> emitters = scene.emissiveMeshCenters();
    emitters.erase(std::remove_if(emitters.begin(), emitters.end(), [](const EmissiveMeshInfo& emitter) {
        return glm::dot(emitter.color, emitter.color) < 0.25f;
    }), emitters.end());

    if (emitters.size() < lightManager.pointLights.size()) {
        std::cerr << "[Light] Emissive lamp alignment skipped: found " << emitters.size()
                  << " emissive mesh center(s), need " << lightManager.pointLights.size() << ".\n";
        return;
    }

    std::vector<bool> used(emitters.size(), false);
    for (PointLight& light : lightManager.pointLights) {
        int bestIndex = -1;
        float bestDistance2 = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < emitters.size(); ++i) {
            if (used[i]) {
                continue;
            }

            const glm::vec3 delta = emitters[i].center - light.position;
            const float distance2 = glm::dot(delta, delta);
            if (distance2 < bestDistance2) {
                bestDistance2 = distance2;
                bestIndex = static_cast<int>(i);
            }
        }

        if (bestIndex >= 0) {
            used[static_cast<std::size_t>(bestIndex)] = true;
            light.position = emitters[static_cast<std::size_t>(bestIndex)].center;
            std::cout << "[Light] " << (light.name.empty() ? "PointLight" : light.name)
                      << " position = (" << light.position.x << ", "
                      << light.position.y << ", " << light.position.z << ")\n";
        }
    }

    std::cout << "[Light] Aligned " << lightManager.pointLights.size()
              << " point light position(s) to emissive lamp mesh centers.\n";
}

void applyBulbTuningToPointLights(LightManager& lightManager)
{
    for (PointLight& light : lightManager.pointLights) {
        light.color = lightManager.tuning.bulbLightColor * lightManager.tuning.bulbLightIntensity;
        light.intensity = lightManager.tuning.bulbLightIntensity;
    }
}

void shutdownImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

} // namespace

int main()
{
    AppConfig appCfg;
    appCfg.width = 1280;
    appCfg.height = 720;
    appCfg.title = "Classroom Renderer";
    appCfg.cameraPos = glm::vec3(0.0f, 2.0f, 12.0f);
    appCfg.cameraFov = 45.0f;
    appCfg.cameraSpeed = 5.0f;
    appCfg.cameraSensitivity = 0.06f;

    Application app(appCfg);
    if (!app.window()) {
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplGlfw_InitForOpenGL(app.window(), true);
    ImGui_ImplOpenGL3_Init("#version 330");

    const std::string vertPath = resolvePath({"shaders/model.vert"});
    const std::string fragPath = resolvePath({"shaders/model.frag"});
    const std::string depthVertPath = resolvePath({"shaders/depth.vert"});
    const std::string depthFragPath = resolvePath({"shaders/depth.frag"});
    const std::string pointDepthVertPath = resolvePath({"shaders/point_depth.vert"});
    const std::string pointDepthFragPath = resolvePath({"shaders/point_depth.frag"});
    const std::string dbgVert = resolvePath({"shaders/debug_line.vert"});
    const std::string dbgFrag = resolvePath({"shaders/debug_line.frag"});
    const std::string fullscreenVert = resolvePath({"shaders/fullscreen.vert"});
    const std::string brightFrag = resolvePath({"shaders/bright_extract.frag"});
    const std::string blurFrag = resolvePath({"shaders/gaussian_blur.frag"});
    const std::string compositeFrag = resolvePath({"shaders/hdr_composite.frag"});
    const std::string ssaoGeometryVert = resolvePath({"shaders/ssao_geometry.vert"});
    const std::string ssaoGeometryFrag = resolvePath({"shaders/ssao_geometry.frag"});
    const std::string ssaoFrag = resolvePath({"shaders/ssao.frag"});
    const std::string ssaoBlurFrag = resolvePath({"shaders/ssao_blur.frag"});
    const std::string lightingConfigPath = resolvePath({"config/lighting_config.json"});
    if (vertPath.empty() || fragPath.empty() || depthVertPath.empty() || depthFragPath.empty()
        || pointDepthVertPath.empty() || pointDepthFragPath.empty()
        || dbgVert.empty() || dbgFrag.empty() || fullscreenVert.empty() || brightFrag.empty()
        || blurFrag.empty() || compositeFrag.empty()
        || ssaoGeometryVert.empty() || ssaoGeometryFrag.empty() || ssaoFrag.empty() || ssaoBlurFrag.empty()) {
        std::cerr << "Required shader files not found.\n";
        shutdownImGui();
        return -1;
    }

    Shader shader(vertPath.c_str(), fragPath.c_str());
    Shader depthShader(depthVertPath.c_str(), depthFragPath.c_str());
    Shader pointDepthShader(pointDepthVertPath.c_str(), pointDepthFragPath.c_str());
    DebugAabbDrawer debugDrawer(Shader(dbgVert.c_str(), dbgFrag.c_str()));
    Shader brightShader(fullscreenVert.c_str(), brightFrag.c_str());
    Shader blurShader(fullscreenVert.c_str(), blurFrag.c_str());
    Shader compositeShader(fullscreenVert.c_str(), compositeFrag.c_str());
    Shader ssaoGeometryShader(ssaoGeometryVert.c_str(), ssaoGeometryFrag.c_str());
    Shader ssaoShader(fullscreenVert.c_str(), ssaoFrag.c_str());
    Shader ssaoBlurShader(fullscreenVert.c_str(), ssaoBlurFrag.c_str());
    if (!shader.isValid() || !depthShader.isValid() || !pointDepthShader.isValid() || !debugDrawer.shader().isValid()
        || !brightShader.isValid() || !blurShader.isValid() || !compositeShader.isValid()
        || !ssaoGeometryShader.isValid() || !ssaoShader.isValid() || !ssaoBlurShader.isValid()) {
        std::cerr << "Shader compilation failed.\n";
        shutdownImGui();
        return -1;
    }

    LightManager lightManager;
    if (lightingConfigPath.empty() || !lightManager.loadConfig(lightingConfigPath)) {
        std::cerr << "[Light] Using built-in fallback lighting values.\n";
    }

    const std::string modelPath = resolvePath({
        "resources/models/partyCarriage.glb",
    });

    Scene scene;
    if (modelPath.empty()) {
        std::cerr << "Model not found. Place partyCarriage.glb at:\n"
                  << "  <project>/resources/models/partyCarriage.glb\n"
                  << "Run the executable from the project root (same folder as CMakeLists.txt).\n";
        shutdownImGui();
        return -1;
    }
    scene.addModel({modelPath, Transform{glm::vec3(0.0f, 0.0f, 0.0f)}, "Model"});

    std::string loadStageText = "Preparing...";
    float loadOverall = 0.0f;
    const auto pumpLoadUi = [&](float normalized, const char* status) {
        if (status != nullptr) {
            loadStageText = status;
        }
        loadOverall = std::clamp(normalized, 0.0f, 1.0f);

        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& loadIo = ImGui::GetIO();
        if (loadIo.DisplaySize.x > 0.0f && loadIo.DisplaySize.y > 0.0f) {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(loadIo.DisplaySize);
            ImGui::Begin("##loading_overlay", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                             | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
            ImGui::TextUnformatted("Classroom Renderer");
            ImGui::Separator();
            ImGui::TextUnformatted("Loading assets - large models may take one to two minutes.");
            ImGui::ProgressBar(loadOverall, ImVec2(-1.0f, 28.0f));
            ImGui::TextWrapped("%s", loadStageText.c_str());
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        restoreGlStateForScene(app.window());
        glfwSwapBuffers(app.window());
        glfwPollEvents();
    };

    std::cout << "[Main] Loading " << scene.entries().size() << " model(s)...\n";
    scene.loadAll(pumpLoadUi);
    alignPointLightsToEmissiveMeshes(lightManager, scene);

    glm::vec3 bmin;
    glm::vec3 bmax;
    glm::vec3 sceneExtent(1.0f);
    if (scene.computeWorldBounds(bmin, bmax)) {
        const glm::vec3 center = 0.5f * (bmin + bmax);
        sceneExtent = glm::max(bmax - bmin, glm::vec3(0.001f));
        Camera& cam = app.camera();
        cam.Position = glm::vec3(center.x, bmin.y + 1.55f, center.z + sceneExtent.z * 0.18f);
        cam.Yaw = -90.0f;
        cam.Pitch = -6.0f;
        cam.MovementSpeed = std::clamp(glm::length(sceneExtent) * 0.08f, 1.5f, 6.0f);
        cam.ProcessMouseMovement(0.0f, 0.0f);
    }

    Renderer renderer(shader, depthShader, app.camera());
    renderer.setPointDepthShader(&pointDepthShader);
    renderer.setLightManager(&lightManager);
    renderer.setLightDirection(glm::normalize(lightManager.sunLight.direction));
    HDRFramebuffer hdrFramebuffer;
    BloomRenderer bloomRenderer(brightShader, blurShader, compositeShader);
    SSAORenderer ssaoRenderer(ssaoGeometryShader, ssaoShader, ssaoBlurShader, app.camera());

    LightingState lighting;
    bool showDemoColliders = false;
    bool useSceneEntryOBBs = true;
    bool collisionEnabled = false;
    bool drawColliders = false;
    bool showSunMarker = true;
    glm::vec3 debugColor{0.25f, 0.95f, 0.35f};
    glm::vec3 lastCameraPosition = app.camera().Position;

    app.run([&](float /*dt*/) {
        handleLightingHotkeys(app.window(), lighting);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        app.setInputBlockedByGui(io.WantCaptureMouse || io.WantCaptureKeyboard);

        ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 360.0f), ImGuiCond_Once);
        ImGui::Begin("Lighting Panel");
        ImGui::SetWindowFontScale(1.35f);
        ImGui::Text("FPS %.1f", static_cast<double>(io.Framerate));
        ImGui::Separator();
        ImGui::Checkbox("Point Lights", &lighting.pointLightsOn);
        ImGui::Checkbox("Show Sun Marker", &showSunMarker);
        ImGui::Checkbox("Collision", &collisionEnabled);
        ImGui::Checkbox("Show Colliders", &drawColliders);
        ImGui::Separator();
        ImGui::TextUnformatted("Ambient Light");
        ImGui::ColorEdit3("Ambient Color", &lightManager.globalAmbient[0]);
        ImGui::Separator();
        ImGui::TextUnformatted("Light Intensity");
        ImGui::SliderFloat("Sun Strength", &lightManager.directionalStrength, 0.0f, 3.0f);
        ImGui::SliderFloat("Shadow Strength", &lightManager.tuning.shadowStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Point Strength", &lightManager.pointLightStrength, 0.0f, 4.0f);
        ImGui::SliderFloat("Flashlight Strength", &lightManager.spotLightStrength, 0.0f, 3.0f);
        ImGui::Separator();
        ImGui::TextUnformatted("Bulb Glow");
        ImGui::Checkbox("Bloom", &lightManager.tuning.bloomEnabled);
        ImGui::SliderFloat("Exposure", &lightManager.tuning.exposure, 0.2f, 3.0f);
        ImGui::SliderFloat("Bloom Threshold", &lightManager.tuning.bloomThreshold, 0.2f, 5.0f);
        ImGui::SliderFloat("Bloom Strength", &lightManager.tuning.bloomStrength, 0.0f, 2.5f);
        ImGui::Checkbox("SSAO", &lightManager.tuning.ssaoEnabled);
        ImGui::SliderFloat("SSAO Radius", &lightManager.tuning.ssaoRadius, 0.05f, 3.0f);
        ImGui::SliderFloat("SSAO Bias", &lightManager.tuning.ssaoBias, 0.0f, 0.15f);
        ImGui::SliderFloat("SSAO Strength", &lightManager.tuning.ssaoStrength, 0.0f, 2.0f);
        ImGui::SliderFloat("Emissive Boost", &lightManager.tuning.emissiveStrengthMultiplier, 0.0f, 8.0f);
        ImGui::SliderFloat("Bulb Light Intensity", &lightManager.tuning.bulbLightIntensity, 0.0f, 120.0f);
        ImGui::Checkbox("Point Shadows", &lightManager.tuning.pointShadowsEnabled);
        ImGui::SliderFloat("Point Shadow Strength", &lightManager.tuning.pointShadowStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Shade Inner", &lightManager.tuning.bulbDownwardInnerCos, -1.0f, 1.0f);
        ImGui::SliderFloat("Shade Outer", &lightManager.tuning.bulbDownwardOuterCos, -1.0f, 1.0f);
        ImGui::ColorEdit3("Bulb Light Color", &lightManager.tuning.bulbLightColor[0]);
        ImGui::DragFloat3("Sun Direction", &lightManager.sunLight.direction[0], 0.02f, -1.0f, 1.0f);
        if (glm::dot(lightManager.sunLight.direction, lightManager.sunLight.direction) < 0.0001f) {
            lightManager.sunLight.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
        }
        if (ImGui::Button("Save Lighting Config", ImVec2(-1.0f, 42.0f)) && !lightingConfigPath.empty()) {
            applyBulbTuningToPointLights(lightManager);
            lightManager.saveConfig(lightingConfigPath);
        }
        ImGui::End();

        std::vector<NamedOBB> colliders;
        std::vector<NamedAABB> debugColliders;
        if (showDemoColliders) {
            appendDemoRoomColliders(debugColliders);
            for (const NamedAABB& box : debugColliders) {
                colliders.push_back({box.name, OBB::fromAABB(box.box)});
            }
        }
        if (useSceneEntryOBBs) {
            std::vector<NamedOBB> fromScene = scene.namedWorldOBBs();
            for (const NamedOBB& collider : fromScene) {
                if (!isUsefulCameraCollider(collider, sceneExtent)) {
                    continue;
                }
                colliders.push_back(collider);
                const std::array<glm::vec3, 8> corners = collider.box.corners();
                glm::vec3 mn(std::numeric_limits<float>::max());
                glm::vec3 mx(std::numeric_limits<float>::lowest());
                for (const glm::vec3& corner : corners) {
                    mn = glm::min(mn, corner);
                    mx = glm::max(mx, corner);
                }
                debugColliders.push_back({collider.name, AABB::fromMinMax(mn, mx)});
            }
        }

        Camera& cam = app.camera();
        if (collisionEnabled) {
            resolveCameraCollision(cam, lastCameraPosition, colliders);
        }
        lastCameraPosition = cam.Position;

        applyBulbTuningToPointLights(lightManager);

        bloomRenderer.enabled = lightManager.tuning.bloomEnabled;
        bloomRenderer.exposure = lightManager.tuning.exposure;
        bloomRenderer.threshold = lightManager.tuning.bloomThreshold;
        bloomRenderer.strength = lightManager.tuning.bloomStrength;
        bloomRenderer.blurIterations = lightManager.tuning.bloomBlurIterations;
        ssaoRenderer.enabled = lightManager.tuning.ssaoEnabled;
        ssaoRenderer.radius = lightManager.tuning.ssaoRadius;
        ssaoRenderer.bias = lightManager.tuning.ssaoBias;
        ssaoRenderer.strength = lightManager.tuning.ssaoStrength;

        int framebufferW = 0;
        int framebufferH = 0;
        glfwGetFramebufferSize(app.window(), &framebufferW, &framebufferH);
        if (framebufferW <= 0 || framebufferH <= 0) {
            framebufferW = appCfg.width;
            framebufferH = appCfg.height;
        }
        hdrFramebuffer.resize(framebufferW, framebufferH);
        bloomRenderer.resize(framebufferW, framebufferH);
        ssaoRenderer.resize(framebufferW, framebufferH);
        ssaoRenderer.render(scene);
        hdrFramebuffer.bind();
        if (lighting.isDay) {
            glClearColor(0.52f, 0.74f, 0.93f, 1.0f);
        } else {
            glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        restoreGlStateForScene(app.window());

        renderer.setLightDirection(glm::normalize(lightManager.sunLight.direction));
        renderer.setShadowStrength(lightManager.tuning.shadowStrength);
        renderer.setSSAO(ssaoRenderer.occlusionTexture(), ssaoRenderer.enabled && ssaoRenderer.isReady(), ssaoRenderer.strength);
        renderer.setRenderTarget(hdrFramebuffer.framebuffer(), hdrFramebuffer.width(), hdrFramebuffer.height());
        syncLightingUniforms(shader, renderer, cam, lighting, lightManager);
        shader.use();
        shader.setFloat("uEmissiveStrengthMultiplier", lightManager.tuning.emissiveStrengthMultiplier);
        renderer.render(scene);
        if (showSunMarker) {
            drawSunMarker(debugDrawer, app.window(), cam, lightManager.sunLight.direction);
        }

        if (drawColliders && !colliders.empty()) {
            int w = 0;
            int h = 0;
            glfwGetFramebufferSize(app.window(), &w, &h);
            if (h <= 0) {
                h = 1;
            }
            const glm::mat4 proj = glm::perspective(
                glm::radians(cam.Zoom),
                static_cast<float>(w) / static_cast<float>(h), 0.1f, 300.0f);
            const glm::mat4 view = cam.GetViewMatrix();
            debugDrawer.draw(proj, view, debugColliders, debugColor);
        }

        bloomRenderer.render(hdrFramebuffer.colorTexture());

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        restoreGlStateForScene(app.window());
    });

    shutdownImGui();
    return 0;
}
