#include "renderer/renderer.h"
#include "renderer/renderer_imgui.h"
#include "core/window.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

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

    // ===================== Performance Stats Panel =====================
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_NoCollapse);

    // FPS (displayed value updates every 0.5s for readability)
    static float displayFps = 0.0f;
    static float displayMs = 0.0f;
    static float displayAvg = 0.0f;
    static float fpsTimer = 0.0f;
    static float fpsAccum = 0.0f;
    static int   fpsSamples = 0;
    // All-time min/max (only update when a new extreme is hit)
    static float allTimeMin = 1e9f;
    static float allTimeMax = 0.0f;
    static bool  fpsWarmedUp = false;  // skip first second to avoid startup spikes

    float currentFps = ImGui::GetIO().Framerate;
    fpsTimer += ImGui::GetIO().DeltaTime;
    fpsAccum += currentFps;
    fpsSamples++;

    // Start tracking min/max after 1s warmup
    static float warmupTimer = 0.0f;
    if (!fpsWarmedUp) {
        warmupTimer += ImGui::GetIO().DeltaTime;
        if (warmupTimer >= 1.0f) fpsWarmedUp = true;
    }
    if (fpsWarmedUp) {
        if (currentFps < allTimeMin) allTimeMin = currentFps;
        if (currentFps > allTimeMax) allTimeMax = currentFps;
    }

    if (fpsTimer >= 0.5f) {
        displayFps = currentFps;
        displayMs = 1000.0f / displayFps;
        displayAvg = fpsAccum / fpsSamples;
        fpsTimer = 0.0f;
        fpsAccum = 0.0f;
        fpsSamples = 0;
    }
    ImGui::Text("FPS: %.1f (%.3f ms)", displayFps, displayMs);
    ImGui::Text("Avg: %.1f  Min: %.1f  Max: %.1f", displayAvg, allTimeMin == 1e9f ? 0.0f : allTimeMin, allTimeMax);

    // Frame time graph (one point every ~50ms, smoothed)
    static float graphHistory[120] = {};
    static int graphOffset = 0;
    static float graphAccum = 0.0f;
    static int graphSamples = 0;
    static float graphTimer = 0.0f;
    graphAccum += 1000.0f / ImGui::GetIO().Framerate;
    graphSamples++;
    graphTimer += ImGui::GetIO().DeltaTime;
    if (graphTimer >= 0.05f) {
        graphHistory[graphOffset] = graphAccum / graphSamples;
        graphOffset = (graphOffset + 1) % 120;
        graphAccum = 0.0f;
        graphSamples = 0;
        graphTimer = 0.0f;
    }
    float maxMs = 0.0f;
    for (int i = 0; i < 120; i++) maxMs = std::max(maxMs, graphHistory[i]);
    ImGui::PlotLines("##frametime", graphHistory, 120, graphOffset,
                     nullptr, 0.0f, maxMs * 1.2f, ImVec2(0, 40));

    ImGui::Separator();

    // VRAM
    size_t meshVram = calculateVRAM();
    if (meshVram >= 1024 * 1024)
        ImGui::Text("Mesh VRAM:  %.2f MB", meshVram / (1024.0f * 1024.0f));
    else
        ImGui::Text("Mesh VRAM:  %.1f KB", meshVram / 1024.0f);

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps{};
    budgetProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    VkPhysicalDeviceMemoryProperties2 memProps2{};
    memProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    memProps2.pNext = &budgetProps;
    vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &memProps2);

    VkDeviceSize totalUsage = 0, totalBudget = 0;
    for (uint32_t i = 0; i < memProps2.memoryProperties.memoryHeapCount; i++) {
        if (memProps2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalUsage += budgetProps.heapUsage[i];
            totalBudget += budgetProps.heapBudget[i];
        }
    }
    if (totalBudget > 0) {
        float usageMB = static_cast<float>(totalUsage / (1024.0 * 1024.0));
        float budgetMB = static_cast<float>(totalBudget / (1024.0 * 1024.0));
        ImGui::Text("GPU VRAM:   %.0f / %.0f MB", usageMB, budgetMB);
        ImGui::ProgressBar(usageMB / budgetMB, ImVec2(-1, 0), "");
    }

    ImGui::Separator();

    // Mesh stats
    uint32_t sceneTriangles = 0;
    uint32_t sceneRendered  = 0;

    ImGui::Text("Base Mesh:");
    ImGui::Indent();
    ImGui::Text("Faces:              %u", heNbFaces);
    ImGui::Text("Vertices:           %u", heNbVertices);
    ImGui::Text("Triangles:          %u", baseMeshTriCount);
    ImGui::Text("Rendered Triangles: %u", baseMeshMode > 0 ? baseMeshTriCount : 0u);
    ImGui::Unindent();

    if (baseMeshMode > 0 && heMeshUploaded) {
        sceneTriangles += baseMeshTriCount;
        sceneRendered  += baseMeshTriCount;
    }

    // Procedural stats (when resurfacing is active) — uses CPU pre-cull cache from recordCommandBuffer
    if (renderResurfacing && heMeshUploaded) {
        ImGui::Separator();
        ImGui::Text("Procedural:");
        ImGui::Indent();

        uint32_t visibleElements = static_cast<uint32_t>(cachedVisibleIndices.size());
        uint32_t totalElements   = cachedTotalElements;
        uint32_t culledElements  = totalElements - visibleElements;

        ImGui::Text("Elements:           %u / %u visible", visibleElements, totalElements);
        if (totalElements > 0) {
            float pct = 100.0f * culledElements / totalElements;
            ImGui::Text("Culled:             %u (%.1f%%)", culledElements, pct);
        }
        if (!enableLod) {
            uint32_t trisPerElement = resolutionM * resolutionN * 2;
            uint32_t procTotal    = totalElements   * trisPerElement;
            uint32_t procRendered = visibleElements * trisPerElement;
            ImGui::Text("Triangles:          %u  (%u/elem)", procTotal, trisPerElement);
            ImGui::Text("Rendered Triangles: %u", procRendered);
            sceneTriangles += procTotal;
            sceneRendered  += procRendered;
        }
        ImGui::Unindent();
    }

    if (benchmarkMeshLoaded) {
        ImGui::Separator();
        ImGui::Text("Benchmark Mesh:");
        ImGui::Indent();
        ImGui::Text("Faces:     %u", benchmarkNbFaces);
        ImGui::Text("Vertices:  %u", benchmarkNbVertices);
        ImGui::Text("Triangles: %u", benchmarkTriCount);
        ImGui::Unindent();
        if (renderBenchmarkMesh) {
            sceneTriangles += benchmarkTriCount;
            sceneRendered  += benchmarkTriCount;
        }
    }

    ImGui::Separator();
    ImGui::Text("Scene:");
    ImGui::Indent();
    ImGui::Text("Triangles:          %u", sceneTriangles);
    ImGui::Text("Rendered Triangles: %llu", static_cast<unsigned long long>(gpuRenderedTriangles));
    ImGui::Unindent();
    ImGui::Text("Resolution: %ux%u", swapChainExtent.width, swapChainExtent.height);

    ImGui::Separator();
    ImGui::Text("Other:");
    ImGui::Indent();
    ImGui::Text("Draw Calls:          %u", frameDrawCalls);
    ImGui::Text("CPU Cull Time:       %.3f ms", cpuCullTimeMs);
    ImGui::Separator();
    ImGui::Text("Task Shaders (CPU):  %u", static_cast<uint32_t>(cachedVisibleIndices.size()));
    if (!enableLod && cachedEstMeshShaders > 0)
        ImGui::Text("Mesh Shaders (est):  %u", cachedEstMeshShaders);
    ImGui::Separator();
    ImGui::Checkbox("GPU Invoc Stats##invoc", &showGPUInvocStats);
    if (showGPUInvocStats) {
        ImGui::Text("Task Shaders (GPU):  %llu", static_cast<unsigned long long>(gpuTaskShaderInvocations));
        ImGui::Text("Mesh Shaders (GPU):  %llu", static_cast<unsigned long long>(gpuMeshShaderInvocations));
    } else {
        ImGui::TextDisabled("(enable to count GPU invocations)");
    }
    ImGui::Unindent();

    ImGui::End();

    // ===================== Gravel Controls Panel =====================
    ImGui::Begin("Gravel Controls");

    // Dynamic mesh list from assets directory
    int meshCount = static_cast<int>(assetMeshNames.size());

    // ===================== Tab Bar =====================
    if (ImGui::BeginTabBar("ModeTabs")) {

        // -------------------- Resurfacing Tab --------------------
        if (ImGui::BeginTabItem("Resurfacing")) {
            uiMode = 0;

            // Presets
            if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (int i = 0; i < LEVEL_PRESET_COUNT; i++) {
                    if (i > 0) ImGui::SameLine();
                    if (ImGui::Button(LEVEL_PRESETS[i].name)) applyPreset(LEVEL_PRESETS[i]);
                }
            }
            ImGui::Separator();

            // Base mesh selector
            if (ImGui::CollapsingHeader("Base Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
                int prev = selectedMesh;
                const char* meshPreview = (selectedMesh >= 0 && selectedMesh < meshCount)
                    ? assetMeshNames[selectedMesh].c_str() : "No meshes found";
                if (ImGui::BeginCombo("Mesh", meshPreview)) {
                    for (int i = 0; i < meshCount; i++) {
                        bool isSelected = (selectedMesh == i);
                        if (ImGui::Selectable(assetMeshNames[i].c_str(), isSelected))
                            selectedMesh = i;
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                bool prevTri = triangulateMesh;
                int prevSubdiv = subdivideLevel;
                ImGui::Checkbox("Triangulate", &triangulateMesh);
                ImGui::SliderInt("Subdivide", &subdivideLevel, 0, 3);
                if (selectedMesh != prev || triangulateMesh != prevTri || subdivideLevel != prevSubdiv)
                    pendingMeshLoad = assetMeshPaths[selectedMesh];
                const char* baseMeshModes[] = { "Off", "Wireframe", "Solid", "Both", "Mask", "Skin" };
                int modeCount = 4;
                if (maskTextureLoaded) modeCount = 5;
                if (skinTextureLoaded) modeCount = 6;
                ImGui::Combo("Display", &baseMeshMode, baseMeshModes, modeCount);
            }
            ImGui::Separator();

            resurfacingPanel.render(*this);
            animationPanel.render(*this);

            ImGui::EndTabItem();
        }

        // -------------------- Benchmark Tab --------------------
        if (ImGui::BeginTabItem("Benchmark")) {
            if (uiMode != 1) {
                // Entering benchmark mode — disable resurfacing
                renderResurfacing = false;
                renderPebbles = false;
                uiMode = 1;
            }

            // Benchmark mesh
            if (ImGui::CollapsingHeader("Benchmark Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
                // Exported meshes dropdown
                static std::vector<std::string> exportNames;
                static std::vector<std::string> exportPaths;
                static int selectedExport = -1;
                static float lastScanTime = -10.0f;

                float now = static_cast<float>(ImGui::GetTime());
                if (now - lastScanTime > 2.0f) {
                    lastScanTime = now;
                    exportNames.clear();
                    exportPaths.clear();
                    if (std::filesystem::is_directory("exports")) {
                        for (const auto& entry : std::filesystem::directory_iterator("exports")) {
                            if (entry.is_regular_file() && entry.path().extension() == ".obj") {
                                exportNames.push_back(entry.path().filename().string());
                                exportPaths.push_back(entry.path().string());
                            }
                        }
                    }
                }

                const char* preview = (selectedExport >= 0 && selectedExport < (int)exportNames.size())
                    ? exportNames[selectedExport].c_str() : "Select exported mesh...";
                if (ImGui::BeginCombo("Exports", preview)) {
                    for (int i = 0; i < (int)exportNames.size(); i++) {
                        bool isSelected = (selectedExport == i);
                        if (ImGui::Selectable(exportNames[i].c_str(), isSelected)) {
                            selectedExport = i;
                            benchmarkMeshPath = exportPaths[i];
                            pendingBenchmarkLoad = benchmarkMeshPath;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (benchmarkMeshLoaded) {
                    if (ImGui::Button("Unload")) {
                        pendingBenchmarkLoad = "__unload__";
                        selectedExport = -1;
                    }
                    ImGui::Checkbox("Render Benchmark", &renderBenchmarkMesh);
                }
            }
            ImGui::EndTabItem();
        }

        // -------------------- Player Tab --------------------
        if (ImGui::BeginTabItem("Player")) {
            if (uiMode != 2) {
                // Entering player mode — load Chainmail Man preset + ground pebbles
                applyPreset(LEVEL_PRESETS[1]);
                renderPathway = true;
                uiMode = 2;
            }

            playerPanel.render(*this);
            ImGui::Separator();

            animationPanel.render(*this);
            ImGui::Separator();

            resurfacingPanel.renderPathway(*this);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    if (ImGui::Button("Clear Scene")) {
        renderResurfacing = false;
        renderPebbles = false;
        renderBenchmarkMesh = false;
        renderPathway = false;
        showControlCage = false;
        showGroundMesh = false;
        baseMeshMode = 0;
        animationPlaying = false;
        thirdPersonMode = false;
        activeCamera = static_cast<CameraBase*>(&freeFlyCamera);
        if (benchmarkMeshLoaded) {
            pendingBenchmarkLoad = "__unload__";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Exit")) {
        glfwSetWindowShouldClose(window.getHandle(), GLFW_TRUE);
    }

    ImGui::End();

    // ===================== Settings Panel =====================
    ImGui::Begin("Settings");

    advancedPanel.render(*this);

    // Lighting controls
    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Light Position", &lightPosition.x, 0.1f, -20.0f, 20.0f);
        ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 10.0f);
        ImGui::ColorEdit3("Ambient Color", &ambientColor.x);
        ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f);
        ImGui::Separator();
        ImGui::Text("Procedural Mesh Material");
        ImGui::ColorEdit3("Base Color##proc", &procBaseColor.x);
        ImGui::SliderFloat("Roughness##proc", &roughness, 0.05f, 1.0f);
        ImGui::SliderFloat("Metallic##proc", &metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("AO##proc", &ao, 0.0f, 1.0f);
        ImGui::SliderFloat("Dielectric F0##proc", &dielectricF0, 0.0f, 0.2f, "%.3f");
        ImGui::SliderFloat("Env Reflection##proc", &envReflection, 0.0f, 1.0f);
        if (dualMeshActive) {
            ImGui::Separator();
            ImGui::Text("Dragon (Base Mesh) Resurfacing Material");
            ImGui::ColorEdit3("Base Color##base", &baseMeshBaseColor.x);
            ImGui::SliderFloat("Roughness##base", &baseMeshRoughness, 0.05f, 1.0f);
            ImGui::SliderFloat("Metallic##base", &baseMeshMetallic, 0.0f, 1.0f);
            ImGui::SliderFloat("AO##base", &baseMeshAo, 0.0f, 1.0f);
            ImGui::SliderFloat("Dielectric F0##base", &baseMeshDielectricF0, 0.0f, 0.2f, "%.3f");
            ImGui::SliderFloat("Env Reflection##base", &baseMeshEnvReflection, 0.0f, 1.0f);
        }
        if (baseMeshMode > 0) {
            ImGui::Separator();
            ImGui::Text("Base Mesh Overlay Material");
            ImGui::ColorEdit3("Base Color##solid", &baseMeshSolidBaseColor.x);
            ImGui::SliderFloat("Roughness##solid", &baseMeshSolidRoughness, 0.05f, 1.0f);
            ImGui::SliderFloat("Metallic##solid", &baseMeshSolidMetallic, 0.0f, 1.0f);
            ImGui::SliderFloat("AO##solid", &baseMeshSolidAo, 0.0f, 1.0f);
            ImGui::SliderFloat("Dielectric F0##solid", &baseMeshSolidDielectricF0, 0.0f, 0.2f, "%.3f");
            ImGui::SliderFloat("Env Reflection##solid", &baseMeshSolidEnvReflection, 0.0f, 1.0f);
        }
        if (baseMeshMode > 0 && dualMeshActive) {
            ImGui::Separator();
            ImGui::Text("Dragon (Base Mesh) Overlay Material");
            ImGui::ColorEdit3("Base Color##solidSec", &secBaseMeshSolidBaseColor.x);
            ImGui::SliderFloat("Roughness##solidSec", &secBaseMeshSolidRoughness, 0.05f, 1.0f);
            ImGui::SliderFloat("Metallic##solidSec", &secBaseMeshSolidMetallic, 0.0f, 1.0f);
            ImGui::SliderFloat("AO##solidSec", &secBaseMeshSolidAo, 0.0f, 1.0f);
            ImGui::SliderFloat("Dielectric F0##solidSec", &secBaseMeshSolidDielectricF0, 0.0f, 0.2f, "%.3f");
            ImGui::SliderFloat("Env Reflection##solidSec", &secBaseMeshSolidEnvReflection, 0.0f, 1.0f);
        }
    }

    // Debug visualization
    if (ImGui::CollapsingHeader("Debug Visualization", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* debugModes[] = {
            "Shading (PBR)",
            "Normals (RGB)",
            "UV Coordinates",
            "Task ID",
            "Element Type (Face/Vertex)",
            "Wireframe"
        };
        int mode = static_cast<int>(debugMode);
        if (ImGui::Combo("Debug Mode", &mode, debugModes, 6)) {
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
                case 5:
                    ImGui::TextDisabled("Triangle edges overlay on shading");
                    break;
            }
            ImGui::Unindent();
        }
    }

    // Camera controls
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        activeCamera->renderImGuiControls();
    }

    ImGui::End();

    // Loading overlay (in progress)
    if (loadingActive) {
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(350, 0));
        ImGui::Begin("##Loading", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("%s", loadingMessage.c_str());
        float progress = static_cast<float>(fmod(ImGui::GetTime() * 0.5, 1.0));
        ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
        ImGui::End();
    }

    // Loading done notification (shows for 2 seconds after load completes)
    if (loadingDone) {
        float elapsed = static_cast<float>(ImGui::GetTime()) - loadingDoneTime;
        if (elapsed < 2.0f) {
            float alpha = elapsed < 1.5f ? 1.0f : 1.0f - (elapsed - 1.5f) / 0.5f;
            ImGui::SetNextWindowBgAlpha(alpha * 0.9f);
            ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f),
                                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(350, 0));
            ImGui::Begin("##LoadDone", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::ProgressBar(1.0f, ImVec2(-1, 0), "");
            ImGui::Text("Done (%.2f s)", loadingDuration);
            ImGui::End();
        } else {
            loadingDone = false;
        }
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void Renderer::applyPresetChainMail() {
    elementType        = 0;       // Torus
    torusMajorR        = 0.965f;
    torusMinorR        = 0.149f;
    userScaling        = 0.628f;
    resolutionM        = 37;
    resolutionN        = 37;
    chainmailMode      = true;
    chainmailTiltAngle = 0.26f;
    chainmailSurfaceOffset = 0.500f;

    // PBR lighting for metallic chainmail
    procBaseColor      = glm::vec3(115.0f/255.0f, 115.0f/255.0f, 115.0f/255.0f);
    ambientColor       = glm::vec3(11.0f/255.0f, 11.0f/255.0f, 11.0f/255.0f);
    ambientIntensity   = 0.311f;
    roughness          = 0.198f;
    metallic           = 1.0f;
    ao                 = 0.697f;
    dielectricF0       = 0.065f;
    envReflection      = 0.572f;
    lightIntensity     = 10.0f;

    // Base mesh overlay material
    baseMeshSolidBaseColor      = glm::vec3(0.0f, 0.0f, 0.0f);
    baseMeshSolidRoughness      = 1.0f;
    baseMeshSolidMetallic       = 1.0f;
    baseMeshSolidAo             = 1.0f;
    baseMeshSolidDielectricF0   = 0.0f;
    baseMeshSolidEnvReflection  = 0.1f;
}

void Renderer::applyPreset(const LevelPreset& preset) {
    // Mesh — find index by path match
    pendingMeshLoad = preset.meshPath;
    selectedMesh = preset.selectedMesh; // fallback
    for (int i = 0; i < static_cast<int>(assetMeshPaths.size()); i++) {
        if (assetMeshPaths[i] == preset.meshPath) {
            selectedMesh = i;
            break;
        }
    }
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
    chainmailSurfaceOffset = preset.chainmailSurfaceOffset;

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
    lightPosition    = preset.lightPosition;
    ambientColor     = preset.ambientColor;
    ambientIntensity = preset.ambientIntensity;
    roughness        = preset.roughness;
    metallic         = preset.metallic;
    ao               = preset.ao;
    dielectricF0     = preset.dielectricF0;
    envReflection    = preset.envReflection;
    lightIntensity   = preset.lightIntensity;
}

