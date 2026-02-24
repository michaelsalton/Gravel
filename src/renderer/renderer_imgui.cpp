#include "renderer/renderer.h"
#include "renderer/renderer_imgui.h"
#include "core/window.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cmath>

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

    // Camera controls
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        camera.renderImGuiControls();
    }

    // Resurfacing controls
    if (ImGui::CollapsingHeader("Resurfacing", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* surfaceTypes[] = {"Torus", "Sphere", "Cone", "Cylinder",
                                      "B-Spline (LUT)", "Bezier (LUT)"};
        int currentType = static_cast<int>(elementType);
        if (ImGui::Combo("Surface Type", &currentType, surfaceTypes, 6)) {
            elementType = static_cast<uint32_t>(currentType);
        }

        // Inline controls for LUT-based surfaces
        if (elementType == 4 || elementType == 5) {
            ImGui::Indent();
            if (!lutLoaded) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                   "No LUT loaded! Use Control Cage section.");
            } else {
                ImGui::Text("Cage: %s (%ux%u)", lutFilename.c_str(),
                            currentLut.Nx, currentLut.Ny);
                if (elementType == 4) {
                    // B-Spline boundary modes
                    bool prevCyclicU = cyclicU, prevCyclicV = cyclicV;
                    ImGui::Checkbox("Cyclic U", &cyclicU);
                    ImGui::SameLine();
                    ImGui::Checkbox("Cyclic V", &cyclicV);
                    if (cyclicU != prevCyclicU || cyclicV != prevCyclicV) {
                        ResurfacingUBO* d = static_cast<ResurfacingUBO*>(resurfacingUBOMapped);
                        d->cyclicU = cyclicU ? 1u : 0u;
                        d->cyclicV = cyclicV ? 1u : 0u;
                    }
                } else {
                    // Bezier degree
                    int prevDegree = bezierDegree;
                    ImGui::SliderInt("Degree", &bezierDegree, 1, 3);
                    const char* degreeNames[] = {"", "Bilinear", "Biquadratic", "Bicubic"};
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s)", degreeNames[bezierDegree]);
                    if (bezierDegree != prevDegree) {
                        ResurfacingUBO* d = static_cast<ResurfacingUBO*>(resurfacingUBOMapped);
                        d->bezierDegree = static_cast<uint32_t>(bezierDegree);
                    }
                }
            }
            ImGui::Unindent();
        }

        ImGui::SliderFloat("Global Scale", &userScaling, 0.1f, 3.0f);

        ImGui::Separator();

        if (elementType == 0) {
            ImGui::Text("Torus Parameters:");
            ImGui::SliderFloat("Major Radius", &torusMajorR, 0.3f, 2.0f);
            ImGui::SliderFloat("Minor Radius", &torusMinorR, 0.05f, 1.0f);
        } else if (elementType == 1) {
            ImGui::Text("Sphere Parameters:");
            ImGui::SliderFloat("Radius", &sphereRadius, 0.1f, 2.0f);
        }

        ImGui::Separator();

        // Resolution controls (tiling allows beyond single-tile limit)
        ImGui::Text("UV Grid Resolution:");
        int resM = static_cast<int>(resolutionM);
        int resN = static_cast<int>(resolutionN);
        if (ImGui::SliderInt("Resolution M", &resM, 2, 64)) {
            resolutionM = static_cast<uint32_t>(resM);
        }
        if (ImGui::SliderInt("Resolution N", &resN, 2, 64)) {
            resolutionN = static_cast<uint32_t>(resN);
        }

        // Compute tile info (mirror shader getDeltaUV logic)
        uint32_t deltaU = resolutionM, deltaV = resolutionN;
        if ((deltaU + 1) * (deltaV + 1) > 256) {
            uint32_t maxD = static_cast<uint32_t>(std::sqrt(256.0f)) - 1;
            deltaU = std::min(deltaU, maxD);
            deltaV = std::min(deltaV, maxD);
        }
        if (deltaU * deltaV * 2 > 256) {
            uint32_t maxD = static_cast<uint32_t>(std::sqrt(256.0f / 2.0f));
            deltaU = std::min(deltaU, maxD);
            deltaV = std::min(deltaV, maxD);
        }
        deltaU = std::max(deltaU, 2u);
        deltaV = std::max(deltaV, 2u);

        uint32_t numTilesU = (resolutionM + deltaU - 1) / deltaU;
        uint32_t numTilesV = (resolutionN + deltaV - 1) / deltaV;
        uint32_t totalTiles = numTilesU * numTilesV;
        uint32_t tileVerts = (deltaU + 1) * (deltaV + 1);
        uint32_t tilePrims = deltaU * deltaV * 2;

        ImGui::Text("Tile: %ux%u (%u verts, %u prims)",
                     deltaU, deltaV, tileVerts, tilePrims);
        ImGui::Text("Tiles: %ux%u = %u per element",
                     numTilesU, numTilesV, totalTiles);

        ImGui::Separator();
        uint32_t totalElements = heNbFaces + heNbVertices;
        ImGui::Text("Elements: %u (%u faces + %u verts)",
                     totalElements, heNbFaces, heNbVertices);
        ImGui::Text("Total mesh tasks: %u", totalElements * totalTiles);
    }

    // Culling controls
    if (ImGui::CollapsingHeader("Culling")) {
        ImGui::Checkbox("Frustum Culling", &enableFrustumCulling);
        ImGui::Checkbox("Back-Face Culling", &enableBackfaceCulling);
        if (enableBackfaceCulling) {
            ImGui::SliderFloat("Threshold", &cullingThreshold, -1.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            if (ImGui::Button("Reset##threshold")) cullingThreshold = 0.0f;
        }
    }

    // Statistics
    if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        uint32_t totalElements = heNbFaces + heNbVertices;
        uint32_t visibleElements = totalElements;

        if (heMeshUploaded && (enableFrustumCulling || enableBackfaceCulling)) {
            // Mirror task shader culling on CPU to count visible elements
            float aspect = static_cast<float>(swapChainExtent.width) /
                           static_cast<float>(swapChainExtent.height);
            glm::mat4 mvp = camera.getProjectionMatrix(aspect) * camera.getViewMatrix();

            visibleElements = 0;
            auto testElement = [&](glm::vec3 pos, glm::vec3 normal, float area) -> bool {
                float radius = std::sqrt(area) * userScaling * 2.0f; // matches computeBoundingRadius

                if (enableFrustumCulling) {
                    glm::vec4 clip = mvp * glm::vec4(pos, 1.0f);
                    if (clip.w <= 0.0f) return false;
                    float cr = radius / clip.w * 2.0f * 1.1f; // margin = 0.1
                    glm::vec3 ndc = glm::vec3(clip) / clip.w;
                    if (ndc.x + cr < -1.0f || ndc.x - cr > 1.0f) return false;
                    if (ndc.y + cr < -1.0f || ndc.y - cr > 1.0f) return false;
                    if (ndc.z + cr <  0.0f || ndc.z - cr > 1.0f) return false;
                }
                if (enableBackfaceCulling) {
                    glm::vec3 viewDir = glm::normalize(camera.position - pos);
                    if (glm::dot(viewDir, normal) <= cullingThreshold) return false;
                }
                return true;
            };

            for (uint32_t i = 0; i < heNbFaces; i++)
                if (testElement(cpuFaceCenters[i], cpuFaceNormals[i], cpuFaceAreas[i]))
                    visibleElements++;
            for (uint32_t i = 0; i < heNbVertices; i++)
                if (testElement(cpuVertexPositions[i], cpuVertexNormals[i], cpuVertexFaceAreas[i]))
                    visibleElements++;
        }

        uint32_t culledElements  = totalElements - visibleElements;
        uint32_t trisPerElement  = resolutionM * resolutionN * 2;
        uint32_t totalTris       = visibleElements * trisPerElement;

        ImGui::Text("Elements:  %u visible / %u total", visibleElements, totalElements);
        if (totalElements > 0) {
            float pct = 100.0f * culledElements / totalElements;
            ImGui::Text("Culled:    %u (%.1f%%)", culledElements, pct);
        }
        ImGui::Text("Triangles: %u  (%u per element)", totalTris, trisPerElement);
    }

    // LOD controls
    if (ImGui::CollapsingHeader("Level of Detail (LOD)")) {
        ImGui::Checkbox("Adaptive LOD", &enableLod);
        if (enableLod) {
            ImGui::SliderFloat("LOD Factor", &lodFactor, 0.1f, 5.0f, "%.2f");
            ImGui::TextDisabled("Base res (M/N above) = target at screen-fill");
            ImGui::TextDisabled("< 1.0 = performance  |  > 1.0 = quality");
        }
    }

    // Control Cage (LUT)
    if (ImGui::CollapsingHeader("Control Cage", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("File: %s", lutFilename.c_str());

        if (lutLoaded) {
            ImGui::Text("Grid: %ux%u (%zu pts)",
                        currentLut.Nx, currentLut.Ny, currentLut.controlPoints.size());
            ImGui::Text("BB min: (%.2f, %.2f, %.2f)",
                        currentLut.bbMin.x, currentLut.bbMin.y, currentLut.bbMin.z);
            ImGui::Text("BB max: (%.2f, %.2f, %.2f)",
                        currentLut.bbMax.x, currentLut.bbMax.y, currentLut.bbMax.z);

        } else {
            ImGui::TextDisabled("No cage loaded.");
        }

        ImGui::Separator();

        if (ImGui::Button("Load scale_4x4.obj")) {
            loadControlCage(std::string(ASSETS_DIR) + "parametric_luts/scale_4x4.obj");
        }
    }

    // Display / VSync
    if (ImGui::CollapsingHeader("Display")) {
        bool prevVsync = vsync;
        ImGui::Checkbox("VSync", &vsync);
        if (vsync != prevVsync) {
            pendingSwapChainRecreation = true;
        }
    }

    // Lighting controls
    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
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
            "Task ID (Per-Element)",
            "Element Type (Face/Vertex)"
        };
        int mode = static_cast<int>(debugMode);
        if (ImGui::Combo("Debug Mode", &mode, debugModes, 5)) {
            debugMode = static_cast<uint32_t>(mode);
        }
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
