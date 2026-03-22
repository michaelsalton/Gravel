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

    // ===================== Main Menu Bar =====================
    float menuBarH = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Window")) {
            if (ImGui::MenuItem(window.isFullscreen() ? "Windowed" : "Fullscreen")) {
                window.toggleFullscreen();
            }
            if (ImGui::MenuItem("Reset Layout")) {
                resetLayout = true;
            }
            ImGui::EndMenu();
        }
        menuBarH = ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }

    // Positions always recalculate (so panels stick to edges on resize)
    // Sizes only set on first use (so user resizing persists), unless reset
    ImGuiCond posCond = ImGuiCond_Always;
    ImGuiCond sizeCond = resetLayout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;

    // ===================== Performance Stats Panel =====================
    float displayH = static_cast<float>(swapChainExtent.height) - menuBarH;
    float displayW = static_cast<float>(swapChainExtent.width);
    float panelW = 310.0f;
    float rightX = displayW - panelW;  // right-column X anchor
    ImGui::SetNextWindowPos(ImVec2(0, menuBarH), posCond);
    ImGui::SetNextWindowSize(ImVec2(panelW, displayH * 0.6f), sizeCond);
    ImGui::Begin("Performance", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                 0);

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
    if (!loadedMeshPath.empty() && std::filesystem::exists(loadedMeshPath)) {
        auto bytes = std::filesystem::file_size(loadedMeshPath);
        if (bytes >= 1024 * 1024)
            ImGui::Text("File Size:          %.1f MB", bytes / (1024.0f * 1024.0f));
        else
            ImGui::Text("File Size:          %.1f KB", bytes / 1024.0f);
    }
    ImGui::Text("Faces:              %u", heNbFaces);
    ImGui::Text("Vertices:           %u", heNbVertices);
    ImGui::Text("Triangles:          %u", baseMeshTriCount);
    {
        uint32_t renderedTris = (baseMeshMode > 0 ? baseMeshTriCount : 0u);
        if (dualMeshActive && dragonBaseMeshMode > 0)
            renderedTris += secondaryHeNbFaces * 2;  // approximate tri count for dragon body
        ImGui::Text("Rendered Triangles: %u", renderedTris);
    }
    ImGui::Unindent();

    if (baseMeshMode > 0 && heMeshUploaded) {
        sceneTriangles += baseMeshTriCount;
        sceneRendered  += baseMeshTriCount;
    }
    if (dualMeshActive && dragonBaseMeshMode > 0 && heMeshUploaded) {
        sceneTriangles += secondaryHeNbFaces * 2;
        sceneRendered  += secondaryHeNbFaces * 2;
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

    float perfH = ImGui::GetWindowSize().y;
    ImGui::End();

    // ===================== Mode Tabs (centered at top) =====================
    {
        float tabW = 400.0f;
        ImGui::SetNextWindowPos(ImVec2((displayW - tabW) * 0.5f, menuBarH), posCond);
        ImGui::SetNextWindowSize(ImVec2(tabW, 0), sizeCond);
        ImGui::Begin("##ModeTabs", nullptr,
                     ImGuiWindowFlags_NoMove  |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                     0);
        if (ImGui::BeginTabBar("ModeTabs")) {
            if (ImGui::BeginTabItem("Resurfacing")) {
                if (uiMode != 0) uiMode = 0;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Benchmark")) {
                if (uiMode != 1) {
                    renderResurfacing = false;
                    renderPebbles = false;
                    uiMode = 1;
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Player")) {
                if (uiMode != 2) {
                    applyPreset(LEVEL_PRESETS[1]);
                    renderPathway = true;
                    uiMode = 2;
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    // ===================== Base Mesh Panel (top-right) =====================
    ImGui::SetNextWindowPos(ImVec2(rightX, menuBarH), posCond);
    ImGui::SetNextWindowSize(ImVec2(panelW, 200), sizeCond);
    ImGui::Begin("Base Mesh", nullptr,
                 ImGuiWindowFlags_NoMove  |
                 0);

    int meshCount = static_cast<int>(assetMeshNames.size());
    {
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
        const char* baseMeshModes[] = { "Off", "Wireframe", "Solid", "Both", "Mask", "Skin", "Colored Faces" };
        int modeCount = 4;
        if (maskTextureLoaded) modeCount = 5;
        if (skinTextureLoaded) modeCount = 6;
        modeCount = std::max(modeCount, 7);  // Colored Faces always available
        if (dualMeshActive) {
            ImGui::Combo("Base Mesh Display", &baseMeshMode, baseMeshModes, modeCount);
            const char* dragonModes[] = { "Off", "Wireframe", "Solid", "Both" };
            ImGui::Combo("Dragon Display", &dragonBaseMeshMode, dragonModes, 4);
        } else {
            ImGui::Combo("Display", &baseMeshMode, baseMeshModes, modeCount);
        }
    }
    float baseMeshPanelH = ImGui::GetWindowSize().y;
    ImGui::End();

    // ===================== Resurfacing Panel (right, below Base Mesh) =====================
    ImGui::SetNextWindowPos(ImVec2(rightX, menuBarH + baseMeshPanelH), posCond);
    ImGui::SetNextWindowSize(ImVec2(panelW, displayH - baseMeshPanelH), sizeCond);
    ImGui::Begin("Resurfacing Controls", nullptr,
                 ImGuiWindowFlags_NoMove );

    if (uiMode == 0) {
        resurfacingPanel.render(*this);
        animationPanel.render(*this);
    }

    // -------------------- Benchmark Content --------------------
    if (uiMode == 1) {
        if (ImGui::CollapsingHeader("Benchmark Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
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
                if (!benchmarkMeshPath.empty() && std::filesystem::exists(benchmarkMeshPath)) {
                    auto bytes = std::filesystem::file_size(benchmarkMeshPath);
                    if (bytes >= 1024 * 1024)
                        ImGui::TextDisabled("  %.1f MB", bytes / (1024.0f * 1024.0f));
                    else
                        ImGui::TextDisabled("  %.1f KB", bytes / 1024.0f);
                }
            }
        }
    }

    // -------------------- Player Content --------------------
    if (uiMode == 2) {
        playerPanel.render(*this);
        ImGui::Separator();

        animationPanel.render(*this);
        ImGui::Separator();

        resurfacingPanel.renderPathway(*this);
    }

    ImGui::End();

    // ===================== Presets Panel (bottom-left col 1) =====================
    ImGui::SetNextWindowPos(ImVec2(0, menuBarH + displayH * 0.6f), posCond);
    ImGui::SetNextWindowSize(ImVec2(panelW, 150), sizeCond);
    ImGui::Begin("Presets", nullptr,
                 ImGuiWindowFlags_NoMove);
    {
        static int selectedLevel = -1;
        const char* preview = (selectedLevel >= 0 && selectedLevel < LEVEL_PRESET_COUNT)
            ? LEVEL_PRESETS[selectedLevel].name : "Select...";
        if (ImGui::BeginCombo("Level", preview)) {
            for (int i = 0; i < LEVEL_PRESET_COUNT; i++) {
                bool isSelected = (selectedLevel == i);
                if (ImGui::Selectable(LEVEL_PRESETS[i].name, isSelected)) {
                    selectedLevel = i;
                    applyPreset(LEVEL_PRESETS[i]);
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Separator();
    {
        const char* materials[] = {"Chainmail", "Pebble", "Dragon", "Playdough", "Grass", "Diamond Plate", "Bubblegum"};
        static int selectedMaterial = -1;
        if (ImGui::Combo("Material", &selectedMaterial, materials, 7)) {
            applyMaterialPreset(selectedMaterial);
        }
    }

    float presetsPanelH = ImGui::GetWindowSize().y;
    ImGui::End();

    // ===================== GRWM Panel (below Presets, left col 1) =====================
    ImGui::SetNextWindowPos(ImVec2(0, menuBarH + displayH * 0.6f + presetsPanelH), posCond);
    ImGui::SetNextWindowSize(ImVec2(panelW, displayH * 0.4f - presetsPanelH), sizeCond);
    ImGui::Begin("GRWM", nullptr,
                 ImGuiWindowFlags_NoMove  |
                 0);
    grwmPanel.render(*this);
    ImGui::End();

    // ===================== Settings Panel (left col 2, top) =====================
    float leftCol2X = panelW;  // second left column
    ImGui::SetNextWindowPos(ImVec2(leftCol2X, menuBarH), posCond);
    ImGui::SetNextWindowSize(ImVec2(panelW, displayH * 0.5f), sizeCond);
    ImGui::Begin("Settings", nullptr,
                 ImGuiWindowFlags_NoMove );

    advancedPanel.render(*this);

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

    // ===================== Lighting & Materials Panel (left col 2, bottom) =====================
    ImGui::SetNextWindowPos(ImVec2(leftCol2X, menuBarH + displayH * 0.5f), posCond);
    ImGui::SetNextWindowSize(ImVec2(panelW, displayH * 0.5f), sizeCond);
    ImGui::Begin("Lighting & Materials", nullptr,
                 ImGuiWindowFlags_NoMove );

    ImGui::DragFloat3("Light Position", &lightPosition.x, 0.1f, -20.0f, 20.0f);
    ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 10.0f);
    ImGui::ColorEdit3("Ambient Color", &ambientColor.x);
    ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f);
    ImGui::Separator();
    ImGui::Text("Procedural Mesh");
    ImGui::ColorEdit3("Base Color##proc", &procBaseColor.x);
    ImGui::SliderFloat("Roughness##proc", &roughness, 0.05f, 1.0f);
    ImGui::SliderFloat("Metallic##proc", &metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("AO##proc", &ao, 0.0f, 1.0f);
    ImGui::SliderFloat("Dielectric F0##proc", &dielectricF0, 0.0f, 0.2f, "%.3f");
    ImGui::SliderFloat("Env Reflection##proc", &envReflection, 0.0f, 1.0f);
    if (dualMeshActive) {
        ImGui::Separator();
        ImGui::Text("Dragon Procedural Surface");
        ImGui::ColorEdit3("Base Color##base", &baseMeshBaseColor.x);
        ImGui::SliderFloat("Roughness##base", &baseMeshRoughness, 0.05f, 1.0f);
        ImGui::SliderFloat("Metallic##base", &baseMeshMetallic, 0.0f, 1.0f);
        ImGui::SliderFloat("AO##base", &baseMeshAo, 0.0f, 1.0f);
        ImGui::SliderFloat("Dielectric F0##base", &baseMeshDielectricF0, 0.0f, 0.2f, "%.3f");
        ImGui::SliderFloat("Env Reflection##base", &baseMeshEnvReflection, 0.0f, 1.0f);
        ImGui::Separator();
        ImGui::Text("Dragon Base Mesh");
        ImGui::ColorEdit3("Base Color##solidSec", &secBaseMeshSolidBaseColor.x);
        ImGui::SliderFloat("Roughness##solidSec", &secBaseMeshSolidRoughness, 0.05f, 1.0f);
        ImGui::SliderFloat("Metallic##solidSec", &secBaseMeshSolidMetallic, 0.0f, 1.0f);
        ImGui::SliderFloat("AO##solidSec", &secBaseMeshSolidAo, 0.0f, 1.0f);
        ImGui::SliderFloat("Dielectric F0##solidSec", &secBaseMeshSolidDielectricF0, 0.0f, 0.2f, "%.3f");
        ImGui::SliderFloat("Env Reflection##solidSec", &secBaseMeshSolidEnvReflection, 0.0f, 1.0f);
    }
    if (baseMeshMode > 0) {
        ImGui::Separator();
        ImGui::Text("Base Mesh");
        ImGui::ColorEdit3("Base Color##solid", &baseMeshSolidBaseColor.x);
        ImGui::SliderFloat("Roughness##solid", &baseMeshSolidRoughness, 0.05f, 1.0f);
        ImGui::SliderFloat("Metallic##solid", &baseMeshSolidMetallic, 0.0f, 1.0f);
        ImGui::SliderFloat("AO##solid", &baseMeshSolidAo, 0.0f, 1.0f);
        ImGui::SliderFloat("Dielectric F0##solid", &baseMeshSolidDielectricF0, 0.0f, 0.2f, "%.3f");
        ImGui::SliderFloat("Env Reflection##solid", &baseMeshSolidEnvReflection, 0.0f, 1.0f);
    }

    ImGui::End();

    // Loading overlay (in progress)
    if (loadingActive) {
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f),
                                posCond, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(350, 0));
        ImGui::Begin("##Loading", nullptr,
                     ImGuiWindowFlags_NoTitleBar  |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     0);
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
                                    posCond, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(350, 0));
            ImGui::Begin("##LoadDone", nullptr,
                         ImGuiWindowFlags_NoTitleBar  |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         0);
            ImGui::ProgressBar(1.0f, ImVec2(-1, 0), "");
            ImGui::Text("Done (%.2f s)", loadingDuration);
            ImGui::End();
        } else {
            loadingDone = false;
        }
    }

    resetLayout = false;

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

void Renderer::applyPresetDragonScales() {
    // Secondary mesh (dragon body) resurfacing
    secondaryElementType        = 5;       // Dragon Scale
    secondaryUserScaling        = 0.770f;
    secondaryResolutionM        = 30;
    secondaryResolutionN        = 30;
    secondaryNormalPerturbation = 0.235f;

    // Secondary mesh (dragon body) PBR material
    baseMeshBaseColor   = glm::vec3(34.0f/255.0f, 78.0f/255.0f, 28.0f/255.0f);
    baseMeshRoughness   = 0.504f;
    baseMeshMetallic    = 0.092f;
    baseMeshAo          = 0.978f;
    baseMeshDielectricF0  = 0.008f;
    baseMeshEnvReflection = 0.368f;

    // Dragon body solid overlay (underneath the scales)
    secBaseMeshSolidBaseColor     = glm::vec3(34.0f/255.0f, 78.0f/255.0f, 28.0f/255.0f);
    secBaseMeshSolidRoughness     = 0.445f;
    secBaseMeshSolidMetallic      = 0.070f;
    secBaseMeshSolidAo            = 1.0f;
    secBaseMeshSolidDielectricF0  = 0.004f;
    secBaseMeshSolidEnvReflection = 0.346f;
}

void Renderer::applyMaterialPreset(int index) {
    // All material presets use the icosphere
    pendingMeshLoad = std::string(ASSETS_DIR) + "base_mesh/shapes/icosphere.obj";
    for (int i = 0; i < static_cast<int>(assetMeshPaths.size()); i++) {
        if (assetMeshPaths[i].find("icosphere") != std::string::npos) {
            selectedMesh = i;
            break;
        }
    }
    triangulateMesh = false;
    baseMeshMode = 0;
    thirdPersonMode = false;
    activeCamera = static_cast<CameraBase*>(&freeFlyCamera);
    animationPlaying = false;
    doSkinning = false;
    pendingPreset = nullptr;

    switch (index) {
    case 0: // Chainmail
        renderResurfacing  = true;
        renderPebbles      = false;
        baseMeshMode       = 2;       // Solid
        elementType        = 0;       // Torus
        torusMajorR        = 0.983f;
        torusMinorR        = 0.159f;
        userScaling        = 0.628f;
        resolutionM        = 28;
        resolutionN        = 29;
        chainmailMode      = true;
        chainmailTiltAngle = 0.27f;
        chainmailSurfaceOffset = 0.500f;
        // Procedural mesh PBR
        procBaseColor      = glm::vec3(73.0f/255.0f);
        roughness          = 0.249f;
        metallic           = 1.0f;
        ao                 = 0.362f;
        dielectricF0       = 0.081f;
        envReflection      = 0.756f;
        // Base mesh PBR
        baseMeshSolidBaseColor     = glm::vec3(0.0f);
        baseMeshSolidRoughness     = 0.050f;
        baseMeshSolidMetallic      = 1.0f;
        baseMeshSolidAo            = 1.0f;
        baseMeshSolidDielectricF0  = 0.0f;
        baseMeshSolidEnvReflection = 0.0f;
        // Lighting
        lightIntensity     = 10.0f;
        ambientColor       = glm::vec3(11.0f/255.0f);
        ambientIntensity   = 0.311f;
        break;

    case 1: // Pebble
        renderResurfacing  = false;
        renderPebbles      = true;
        baseMeshMode       = 2;       // Solid
        chainmailMode      = false;
        pebbleUBO.subdivisionLevel   = 6;
        pebbleUBO.subdivOffset       = 0;
        pebbleUBO.extrusionAmount    = 0.090f;
        pebbleUBO.extrusionVariation = 0.324f;
        pebbleUBO.roundness          = 1.90f;
        pebbleUBO.doNoise            = 1;
        pebbleUBO.noiseAmplitude     = 0.005f;
        pebbleUBO.noiseFrequency     = 10.0f;
        pebbleUBO.normalOffset       = 0.19f;
        pebbleUBO.useCulling         = 0;
        pebbleUBO.useLod             = 1;
        pebbleUBO.lodFactor          = 1.0f;
        pebbleUBO.allowLowLod        = 1;
        // Procedural mesh PBR
        procBaseColor      = glm::vec3(50.0f/255.0f);
        roughness          = 1.0f;
        metallic           = 0.097f;
        ao                 = 0.946f;
        dielectricF0       = 0.197f;
        envReflection      = 0.054f;
        // Base mesh PBR
        baseMeshSolidBaseColor     = glm::vec3(50.0f/255.0f);
        baseMeshSolidRoughness     = 0.291f;
        baseMeshSolidMetallic      = 0.027f;
        baseMeshSolidAo            = 0.892f;
        baseMeshSolidDielectricF0  = 0.199f;
        baseMeshSolidEnvReflection = 0.546f;
        // Lighting
        lightIntensity     = 3.244f;
        ambientColor       = glm::vec3(139.0f/255.0f);
        ambientIntensity   = 0.092f;
        break;

    case 2: // Dragon Scale
        renderResurfacing  = true;
        renderPebbles      = false;
        baseMeshMode       = 2;       // Solid
        elementType        = 5;       // Dragon Scale
        userScaling        = 0.770f;
        resolutionM        = 30;
        resolutionN        = 30;
        chainmailMode      = false;
        normalPerturbation = 0.235f;
        // Procedural mesh PBR
        procBaseColor      = glm::vec3(34.0f/255.0f, 78.0f/255.0f, 28.0f/255.0f);
        roughness          = 0.504f;
        metallic           = 0.092f;
        ao                 = 0.978f;
        dielectricF0       = 0.008f;
        envReflection      = 0.368f;
        // Base mesh PBR
        baseMeshSolidBaseColor     = glm::vec3(34.0f/255.0f, 78.0f/255.0f, 28.0f/255.0f);
        baseMeshSolidRoughness     = 0.445f;
        baseMeshSolidMetallic      = 0.070f;
        baseMeshSolidAo            = 1.0f;
        baseMeshSolidDielectricF0  = 0.004f;
        baseMeshSolidEnvReflection = 0.346f;
        // Lighting
        lightIntensity     = 5.0f;
        ambientColor       = glm::vec3(1.0f);
        ambientIntensity   = 0.405f;
        break;

    case 3: // Playdough
        renderResurfacing  = false;
        renderPebbles      = true;
        baseMeshMode       = 2;       // Solid
        chainmailMode      = false;
        pebbleUBO.subdivisionLevel   = 8;
        pebbleUBO.subdivOffset       = 0;
        pebbleUBO.extrusionAmount    = 0.039f;
        pebbleUBO.extrusionVariation = 0.0f;
        pebbleUBO.roundness          = 0.56f;
        pebbleUBO.doNoise            = 1;
        pebbleUBO.noiseAmplitude     = 0.062f;
        pebbleUBO.noiseFrequency     = 8.5f;
        pebbleUBO.normalOffset       = 0.32f;
        pebbleUBO.useCulling         = 1;
        pebbleUBO.cullingThreshold   = 0.32f;
        pebbleUBO.useLod             = 1;
        pebbleUBO.lodFactor          = 1.0f;
        pebbleUBO.allowLowLod        = 1;
        // Procedural mesh PBR
        procBaseColor      = glm::vec3(34.0f/255.0f, 20.0f/255.0f, 18.0f/255.0f);
        roughness          = 0.292f;
        metallic           = 0.027f;
        ao                 = 0.892f;
        dielectricF0       = 0.199f;
        envReflection      = 0.544f;
        // Base mesh PBR
        baseMeshSolidBaseColor     = glm::vec3(34.0f/255.0f, 20.0f/255.0f, 18.0f/255.0f);
        baseMeshSolidRoughness     = 0.291f;
        baseMeshSolidMetallic      = 0.027f;
        baseMeshSolidAo            = 0.892f;
        baseMeshSolidDielectricF0  = 0.199f;
        baseMeshSolidEnvReflection = 0.546f;
        // Lighting
        lightIntensity     = 6.541f;
        ambientColor       = glm::vec3(1.0f);
        ambientIntensity   = 0.541f;
        break;

    case 4: // Grass
        renderResurfacing  = true;
        renderPebbles      = false;
        baseMeshMode       = 2;       // Solid
        elementType        = 6;       // Straw
        userScaling        = 3.0f;
        resolutionM        = 42;
        resolutionN        = 42;
        chainmailMode      = false;
        strawBaseRadius    = 0.023f;
        strawTaperPower    = 3.746f;
        strawBendAmount    = 0.676f;
        strawBendDirection = 0.0f;
        strawBendRandomness = 0.735f;
        // Procedural mesh PBR
        procBaseColor      = glm::vec3(15.0f/255.0f, 48.0f/255.0f, 10.0f/255.0f);
        roughness          = 0.504f;
        metallic           = 0.0f;
        ao                 = 1.0f;
        dielectricF0       = 0.0f;
        envReflection      = 0.0f;
        // Base mesh PBR
        baseMeshSolidBaseColor     = glm::vec3(15.0f/255.0f, 48.0f/255.0f, 10.0f/255.0f);
        baseMeshSolidRoughness     = 0.461f;
        baseMeshSolidMetallic      = 0.0f;
        baseMeshSolidAo            = 1.0f;
        baseMeshSolidDielectricF0  = 0.0f;
        baseMeshSolidEnvReflection = 0.0f;
        // Lighting
        lightIntensity     = 5.0f;
        ambientColor       = glm::vec3(1.0f);
        ambientIntensity   = 0.405f;
        break;

    case 5: // Diamond Plate
        renderResurfacing  = true;
        renderPebbles      = false;
        baseMeshMode       = 2;       // Solid
        elementType        = 7;       // Stud
        userScaling        = 0.12f;
        resolutionM        = 16;
        resolutionN        = 16;
        chainmailMode      = false;
        studElongation         = 3.0f;
        studHeight             = 0.15f;
        studPower              = 2.0f;
        studRotation           = 0.785f;  // 45 degrees
        studRotationRandomness = 0.0f;
        studTreadPlate         = true;
        // Procedural mesh PBR (brushed metal)
        procBaseColor      = glm::vec3(0.55f, 0.55f, 0.55f);
        roughness          = 0.35f;
        metallic           = 1.0f;
        ao                 = 0.8f;
        dielectricF0       = 0.04f;
        envReflection      = 0.5f;
        // Base mesh PBR
        baseMeshSolidBaseColor     = glm::vec3(0.45f, 0.45f, 0.45f);
        baseMeshSolidRoughness     = 0.4f;
        baseMeshSolidMetallic      = 1.0f;
        baseMeshSolidAo            = 1.0f;
        baseMeshSolidDielectricF0  = 0.04f;
        baseMeshSolidEnvReflection = 0.5f;
        // Lighting
        lightIntensity     = 5.0f;
        ambientColor       = glm::vec3(0.8f);
        ambientIntensity   = 0.4f;
        break;

    case 6: // Bubblegum
        renderResurfacing  = true;
        renderPebbles      = false;
        baseMeshMode       = 0;       // Off
        elementType        = 4;       // Hemisphere
        userScaling        = 0.576f;
        resolutionM        = 64;
        resolutionN        = 64;
        chainmailMode      = false;
        // Procedural mesh PBR
        procBaseColor      = glm::vec3(168.0f/255.0f, 33.0f/255.0f, 33.0f/255.0f);
        roughness          = 0.250f;
        metallic           = 0.0f;
        ao                 = 0.854f;
        dielectricF0       = 0.132f;
        envReflection      = 0.805f;
        // Lighting
        lightIntensity     = 8.243f;
        ambientColor       = glm::vec3(0.0f);
        ambientIntensity   = 1.0f;
        break;
    }
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

