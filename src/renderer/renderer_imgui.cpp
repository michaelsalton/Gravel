#include "renderer/renderer.h"
#include "renderer/renderer_imgui.h"
#include "core/window.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

void Renderer::initImGui() {
    // Create dedicated descriptor pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr,
                                &imguiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = BUILD_DIR "imgui.ini";

    ImGui::StyleColorsDark();

    // Initialize GLFW backend
    ImGui_ImplGlfw_InitForVulkan(window.getHandle(), true);

    // Initialize Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamilyIndices.graphicsFamily.value();
    initInfo.Queue = graphicsQueue;
    initInfo.DescriptorPool = imguiDescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = static_cast<uint32_t>(swapChainImages.size());
    initInfo.UseDynamicRendering = false;

    initInfo.RenderPass = renderPass;
    initInfo.Subpass = 0;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    std::cout << "ImGui initialized" << std::endl;
}

void Renderer::cleanupImGui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
}

void Renderer::renderImGui(VkCommandBuffer cmd) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Gravel Controls");

    // FPS Counter
    ImGui::Text("FPS: %.1f (%.3f ms/frame)",
                ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);
    ImGui::Separator();

    // Presets
    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < LEVEL_PRESET_COUNT; i++) {
            if (i > 0) ImGui::SameLine();
            if (ImGui::Button(LEVEL_PRESETS[i].name)) applyPreset(LEVEL_PRESETS[i]);
        }
    }
    ImGui::Separator();

    const char* meshNames[] = { "Cube", "Plane (3x3)", "Plane (5x5)", "Plane (Pentagon)", "Sphere", "Sphere HD", "Icosphere", "Dragon 8K", "Dragon Coat", "Boy", "Man", "Man 2" };
    const char* meshPaths[] = {
        ASSETS_DIR "shapes/cube.obj",
        ASSETS_DIR "shapes/plane.obj",
        ASSETS_DIR "shapes/plane5x5.obj",
        ASSETS_DIR "shapes/plane_pentagon.obj",
        ASSETS_DIR "shapes/sphere.obj",
        ASSETS_DIR "shapes/sphere_hd.obj",
        ASSETS_DIR "shapes/icosphere.obj",
        ASSETS_DIR "dragon/dragon_8k.obj",
        ASSETS_DIR "dragon/dragon_coat.obj",
        ASSETS_DIR "low-poly/boy.obj",
        ASSETS_DIR "man/man.obj",
        ASSETS_DIR "man2/man2.obj"
    };
    constexpr int meshCount = 12;

    // Base mesh selector
    if (ImGui::CollapsingHeader("Base Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        int prev = selectedMesh;
        ImGui::Combo("Mesh", &selectedMesh, meshNames, meshCount);
        bool prevTri = triangulateMesh;
        ImGui::Checkbox("Triangulate", &triangulateMesh);
        if (selectedMesh != prev || triangulateMesh != prevTri)
            pendingMeshLoad = meshPaths[selectedMesh];
        const char* baseMeshModes[] = { "Off", "Wireframe", "Solid", "Both", "Mask", "Skin" };
        int modeCount = 4;
        if (maskTextureLoaded) modeCount = 5;
        if (skinTextureLoaded) modeCount = 6;
        ImGui::Combo("Display", &baseMeshMode, baseMeshModes, modeCount);
    }
    ImGui::Separator();

    // Benchmark mesh (static OBJ for A/B comparison)
    if (ImGui::CollapsingHeader("Benchmark Mesh")) {
        static char benchPath[256];
        if (benchPath[0] == '\0') {
            strncpy(benchPath, benchmarkMeshPath.c_str(), sizeof(benchPath) - 1);
        }
        ImGui::InputText("OBJ Path", benchPath, sizeof(benchPath));
        if (ImGui::Button("Load Benchmark")) {
            benchmarkMeshPath = benchPath;
            pendingBenchmarkLoad = benchmarkMeshPath;
        }
        if (benchmarkMeshLoaded) {
            ImGui::SameLine();
            if (ImGui::Button("Unload")) {
                pendingBenchmarkLoad = "__unload__";
            }
        }
        if (benchmarkMeshLoaded) {
            ImGui::Checkbox("Render Benchmark", &renderBenchmarkMesh);
            ImGui::Text("Triangles: %u", benchmarkTriCount);
            ImGui::Text("Faces (HE): %u  Vertices: %u", benchmarkNbFaces, benchmarkNbVertices);
        }
    }
    ImGui::Separator();

    playerPanel.render(*this);
    ImGui::Separator();

    resurfacingPanel.render(*this);
    animationPanel.render(*this);

    // Lighting controls
    if (ImGui::CollapsingHeader("Lighting")) {
        ImGui::DragFloat3("Light Position", &lightPosition.x, 0.1f, -20.0f, 20.0f);
        ImGui::ColorEdit3("Ambient Color", &ambientColor.x);
        ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Diffuse", &diffuseIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Specular", &specularIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Shininess", &shininess, 1.0f, 128.0f);
    }

    // Debug visualization
    if (ImGui::CollapsingHeader("Debug Visualization")) {
        const char* debugModes[] = {
            "Shading (Blinn-Phong)",
            "Normals (RGB)",
            "UV Coordinates",
            "Task ID",
            "Element Type (Face/Vertex)"
        };
        int mode = static_cast<int>(debugMode);
        if (ImGui::Combo("Debug Mode", &mode, debugModes, 5)) {
            debugMode = static_cast<uint32_t>(mode);
        }

        // Show mode-specific info
        if (debugMode > 0) {
            ImGui::Indent();
            switch (debugMode) {
                case 1:
                    ImGui::TextDisabled("RGB colors represent surface normals");
                    break;
                case 2:
                    ImGui::TextDisabled("Red = U axis, Green = V axis");
                    break;
                case 3:
                    ImGui::TextDisabled("Unique color per surface element");
                    break;
                case 4:
                    ImGui::TextDisabled("Red = Vertex, Blue = Face");
                    break;
            }
            ImGui::Unindent();
        }
    }

    // Camera controls
    if (ImGui::CollapsingHeader("Camera")) {
        activeCamera->renderImGuiControls();
    }

    advancedPanel.render(*this);

    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void Renderer::applyPresetChainMail() {
    elementType        = 0;       // Torus
    torusMajorR        = 0.814f;
    torusMinorR        = 0.158f;
    userScaling        = 0.602f;
    resolutionM        = 24;
    resolutionN        = 6;
    chainmailMode      = true;
    chainmailTiltAngle = 0.08f;

    // Metallic silver lighting
    ambientColor       = glm::vec3(11.0f/255.0f, 11.0f/255.0f, 11.0f/255.0f);
    ambientIntensity   = 0.717f;
    diffuseIntensity   = 0.666f;
    specularIntensity  = 1.0f;
    shininess          = 11.237f;

    // Metallic shader params
    metalF0            = 0.65f;
    envReflection      = 0.35f;
    metalDiffuse       = 0.3f;
}

void Renderer::applyPreset(const LevelPreset& preset) {
    // Mesh
    selectedMesh = preset.selectedMesh;
    pendingMeshLoad = preset.meshPath;
    triangulateMesh = preset.triangulateMesh;
    baseMeshMode = preset.baseMeshMode;

    // Resurfacing
    renderResurfacing = preset.renderResurfacing;
    renderPebbles = false;
    elementType = preset.elementType;
    torusMajorR = preset.torusMajorR;
    torusMinorR = preset.torusMinorR;
    sphereRadius = preset.sphereRadius;
    userScaling = preset.userScaling;
    resolutionM = preset.resolutionM;
    resolutionN = preset.resolutionN;

    // Chainmail
    chainmailMode = preset.chainmailMode;
    chainmailTiltAngle = preset.chainmailTiltAngle;

    // Camera / Player
    thirdPersonMode = preset.thirdPersonMode;
    activeCamera = thirdPersonMode
        ? static_cast<CameraBase*>(&orbitCamera)
        : static_cast<CameraBase*>(&freeFlyCamera);
    orbitCamera.distance = preset.orbitCameraDistance;
    if (thirdPersonMode) {
        orbitCamera.setTarget(player.position + glm::vec3(0.0f, 1.5f, 0.0f));
    }

    // Animation (also applied after mesh load since cleanupMeshSkeleton resets these)
    doSkinning = preset.doSkinning;
    animationPlaying = preset.animationPlaying;
    animationSpeed = preset.animationSpeed;

    // Store preset pointer for post-load re-application
    pendingPreset = &preset;

    // Lighting
    lightPosition = preset.lightPosition;
    ambientColor = preset.ambientColor;
    ambientIntensity = preset.ambientIntensity;
    diffuseIntensity = preset.diffuseIntensity;
    specularIntensity = preset.specularIntensity;
    shininess = preset.shininess;

    // Metallic
    metalF0 = preset.metalF0;
    envReflection = preset.envReflection;
    metalDiffuse = preset.metalDiffuse;
}

