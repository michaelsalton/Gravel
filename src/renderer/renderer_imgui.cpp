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

    initInfo.PipelineInfoMain.RenderPass = renderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

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

    const char* meshNames[] = { "Cube", "Plane (3x3)", "Plane (5x5)", "Sphere", "Sphere HD", "Icosphere", "Dragon 8K", "Boy" };
    const char* meshPaths[] = {
        ASSETS_DIR "cube.obj",
        ASSETS_DIR "plane.obj",
        ASSETS_DIR "plane5x5.obj",
        ASSETS_DIR "sphere.obj",
        ASSETS_DIR "sphere_hd.obj",
        ASSETS_DIR "icosphere.obj",
        ASSETS_DIR "dragon_8k.obj",
        ASSETS_DIR "boy.obj"
    };
    constexpr int meshCount = 8;

    // Base mesh selector
    if (ImGui::CollapsingHeader("Base Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        int prev = selectedMesh;
        ImGui::Combo("Mesh", &selectedMesh, meshNames, meshCount);
        bool prevTri = triangulateMesh;
        ImGui::Checkbox("Triangulate", &triangulateMesh);
        if (selectedMesh != prev || triangulateMesh != prevTri)
            loadMesh(meshPaths[selectedMesh]);
        const char* baseMeshModes[] = { "Off", "Wireframe", "Solid", "Both" };
        ImGui::Combo("Display", &baseMeshMode, baseMeshModes, 4);
    }
    ImGui::Separator();

    // Resurfacing controls
    if (ImGui::CollapsingHeader("Resurfacing", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* surfaceTypes[] = {"Torus", "Sphere", "Cone", "Cylinder"};
        int currentType = static_cast<int>(elementType);
        if (ImGui::Combo("Surface Type", &currentType, surfaceTypes, 4)) {
            elementType = static_cast<uint32_t>(currentType);
        }

        ImGui::SliderFloat("Global Scale", &userScaling, 0.01f, 3.0f);

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

        // Chainmail mode
        {
            bool prev = chainmailMode;
            ImGui::Checkbox("Chainmail Mode", &chainmailMode);
            if (chainmailMode && !prev) {
                applyPresetChainMail();
                if (triangulateMesh) {
                    triangulateMesh = false;
                    loadMesh(meshPaths[selectedMesh]);
                }
            }
        }
        if (chainmailMode) {
            ImGui::Indent();
            ImGui::SliderFloat("Lean Amount", &chainmailTiltAngle, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            ImGui::Text("(0=flat, 1=full)");
            ImGui::Text("Metallic Shading:");
            ImGui::SliderFloat("Metal Reflectance", &metalF0, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Env Reflection", &envReflection, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Metal Diffuse", &metalDiffuse, 0.0f, 1.0f, "%.2f");
            ImGui::Unindent();
        }

        ImGui::Separator();
        uint32_t totalElements = heNbFaces + heNbVertices;
        ImGui::Text("Elements: %u (%u faces + %u verts)",
                     totalElements, heNbFaces, heNbVertices);
        ImGui::Text("Total mesh tasks: %u", totalElements * totalTiles);
    }

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
            "Task/Face ID (Per-Element)",
            "Element Type / LOD"
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
        camera.renderImGuiControls();
    }

    // Advanced: Culling, LOD, Display, Statistics
    if (ImGui::CollapsingHeader("Advanced")) {
        // Display
        bool prevVsync = vsync;
        ImGui::Checkbox("VSync", &vsync);
        if (vsync != prevVsync) {
            pendingSwapChainRecreation = true;
        }

        ImGui::Separator();

        // Culling
        ImGui::Checkbox("Frustum Culling", &enableFrustumCulling);
        ImGui::Checkbox("Back-Face Culling", &enableBackfaceCulling);
        if (enableBackfaceCulling) {
            ImGui::SliderFloat("Threshold", &cullingThreshold, -1.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            if (ImGui::Button("Reset##threshold")) cullingThreshold = 0.0f;
        }

        ImGui::Separator();

        // LOD
        ImGui::Checkbox("Adaptive LOD", &enableLod);
        if (enableLod) {
            ImGui::SliderFloat("LOD Factor", &lodFactor, 0.1f, 5.0f, "%.2f");
            ImGui::TextDisabled("< 1.0 = performance  |  > 1.0 = quality");
        }

        ImGui::Separator();

        // Statistics
        {
            uint32_t totalElements = heNbFaces + heNbVertices;
            uint32_t visibleElements = totalElements;

            if (heMeshUploaded && (enableFrustumCulling || enableBackfaceCulling)) {
                float aspect = static_cast<float>(swapChainExtent.width) /
                               static_cast<float>(swapChainExtent.height);
                glm::mat4 mvp = camera.getProjectionMatrix(aspect) * camera.getViewMatrix();

                visibleElements = 0;
                auto testElement = [&](glm::vec3 pos, glm::vec3 normal, float area) -> bool {
                    float radius = std::sqrt(area) * userScaling * 2.0f;
                    if (enableFrustumCulling) {
                        glm::vec4 clip = mvp * glm::vec4(pos, 1.0f);
                        if (clip.w <= 0.0f) return false;
                        float cr = radius / clip.w * 2.0f * 1.1f;
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

            uint32_t culledElements = totalElements - visibleElements;
            uint32_t trisPerElement = resolutionM * resolutionN * 2;
            uint32_t totalTris = visibleElements * trisPerElement;

            ImGui::Text("Elements:  %u visible / %u total", visibleElements, totalElements);
            if (totalElements > 0) {
                float pct = 100.0f * culledElements / totalElements;
                ImGui::Text("Culled:    %u (%.1f%%)", culledElements, pct);
            }
            ImGui::Text("Triangles: %u  (%u per element)", totalTris, trisPerElement);
        }
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void Renderer::applyPresetChainMail() {
    elementType        = 0;       // Torus
    torusMajorR        = 0.693f;
    torusMinorR        = 0.185f;
    userScaling        = 1.219f;
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

