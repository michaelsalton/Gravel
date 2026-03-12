#include "renderer/renderer.h"
#include "renderer/MeshExport.h"
#include "loaders/ObjWriter.h"
#include "loaders/ObjLoader.h"
#include "loaders/GltfLoader.h"
#include "core/window.h"
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <array>
#include <cstring>
#include <cmath>
#include <chrono>

Renderer::Renderer(Window& window) : window(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    loadMeshShaderFunctions();
    createCommandPool();
    createSwapChain();
    createImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();

    // Pipeline statistics query pool for real-time triangle counting
    {
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        queryPoolInfo.queryCount = STATS_QUERY_COUNT;
        queryPoolInfo.pipelineStatistics =
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT;
        if (vkCreateQueryPool(device, &queryPoolInfo, nullptr, &statsQueryPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create statistics query pool!");
        }
    }

    createDescriptorSetLayouts();
    createPipelineLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createGraphicsPipeline();
    createBenchmarkPipeline();
    createSamplers();
    generateGroundPlane(groundPlaneCellSize);
    scanAssetMeshes();
    initImGui();
}

Renderer::~Renderer() {
    cleanupImGui();
    if (statsQueryPool != VK_NULL_HANDLE)
        vkDestroyQueryPool(device, statsQueryPool, nullptr);
    cleanupExportPipelines();
    cleanupBenchmarkMesh();
    cleanupGroundMesh();
    cleanupSecondaryMesh();
    cleanupMeshSkeleton();
    cleanupMeshTextures();
    if (benchmarkPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device, benchmarkPipeline, nullptr);
    if (benchmarkPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, benchmarkPipelineLayout, nullptr);
    vkDestroyPipeline(device, pebbleCagePipeline, nullptr);
    vkDestroyPipeline(device, pebblePipeline, nullptr);
    vkDestroyPipeline(device, baseMeshSolidPipeline, nullptr);
    vkDestroyPipeline(device, baseMeshPipeline, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);

    // Cleanup half-edge SSBO buffers (StorageBuffer destructors handle their own cleanup)
    heVec4Buffers.clear();
    heVec2Buffers.clear();
    heIntBuffers.clear();
    heFloatBuffers.clear();

    if (meshInfoBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, meshInfoBuffer, nullptr);
        vkFreeMemory(device, meshInfoMemory, nullptr);
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, viewUBOBuffers[i], nullptr);
        vkFreeMemory(device, viewUBOMemory[i], nullptr);
        vkDestroyBuffer(device, shadingUBOBuffers[i], nullptr);
        vkFreeMemory(device, shadingUBOMemory[i], nullptr);
        vkDestroyBuffer(device, visibleIndicesBuffers[i], nullptr);
        vkFreeMemory(device, visibleIndicesMemory[i], nullptr);
    }

    if (resurfacingUBOBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, resurfacingUBOBuffer, nullptr);
        vkFreeMemory(device, resurfacingUBOMemory, nullptr);
    }

    if (pebbleUBOBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, pebbleUBOBuffer, nullptr);
        vkFreeMemory(device, pebbleUBOMemory, nullptr);
    }

    if (linearSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, linearSampler, nullptr);
    }
    if (nearestSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, nearestSampler, nullptr);
    }

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, sceneSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, halfEdgeSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, perObjectSetLayout, nullptr);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    cleanupSwapChain();

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
    }

    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }

    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
    }

    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }

    if (enableValidationLayers && debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, nullptr);
        }
    }

    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }
}

void Renderer::beginFrame() {
    // Check if any heavy operation is pending — show loading overlay first frame,
    // then do the actual work on the next frame
    bool hasPendingWork = !pendingMeshLoad.empty() ||
                          (!pendingBenchmarkLoad.empty() && pendingBenchmarkLoad != "__unload__") ||
                          pendingExport;

    if (hasPendingWork && !loadingActive) {
        // First frame: just set loading flag, let this frame render the overlay
        loadingActive = true;
        loadingFrameCount = 0;
        if (!pendingMeshLoad.empty())
            loadingMessage = "Loading mesh...";
        else if (!pendingBenchmarkLoad.empty())
            loadingMessage = "Loading benchmark mesh...";
        else if (pendingExport)
            loadingMessage = "Exporting mesh...";
        loadingStartTime = static_cast<float>(glfwGetTime());
        // Don't process the work yet — fall through to render a frame with the overlay
    } else if (loadingActive && loadingFrameCount < 2) {
        // Wait for overlay to be visible on screen (need 2 frames: render + present)
        loadingFrameCount++;
        // Fall through to render another frame with the overlay
    } else if (loadingActive) {
        // Overlay has been shown for 2 frames, now do the actual work
        if (pendingGroundRegenerate) {
            pendingGroundRegenerate = false;
            vkDeviceWaitIdle(device);
            generateGroundPlane(groundPlaneCellSize);
        }

        if (!pendingMeshLoad.empty()) {
            std::string path = std::move(pendingMeshLoad);
            pendingMeshLoad.clear();
            loadMesh(path);

            if (pendingPreset) {
                doSkinning = pendingPreset->doSkinning;
                animationPlaying = pendingPreset->animationPlaying;
                animationSpeed = pendingPreset->animationSpeed;
                baseMeshMode = pendingPreset->baseMeshMode;
                pendingPreset = nullptr;
            }
        }

        if (!pendingBenchmarkLoad.empty()) {
            std::string path = std::move(pendingBenchmarkLoad);
            pendingBenchmarkLoad.clear();
            if (path == "__unload__") {
                vkDeviceWaitIdle(device);
                cleanupBenchmarkMesh();
                renderBenchmarkMesh = false;
            } else {
                loadBenchmarkMesh(path);
            }
        }

        if (pendingExport) {
            pendingExport = false;
            try {
                auto pos = exportFilePath.find_last_of("/\\");
                if (pos != std::string::npos) {
                    std::filesystem::create_directories(exportFilePath.substr(0, pos));
                }
                exportProceduralMesh(exportFilePath, exportMode);
                lastExportStatus = "Exported: " + exportFilePath;
            } catch (const std::exception& e) {
                lastExportStatus = std::string("Export failed: ") + e.what();
            }
        }

        loadingDuration = static_cast<float>(glfwGetTime()) - loadingStartTime;
        loadingActive = false;
        loadingDone = true;
        loadingDoneTime = static_cast<float>(glfwGetTime());
    } else {
        // No loading — process lightweight deferred ops
        if (pendingGroundRegenerate) {
            pendingGroundRegenerate = false;
            vkDeviceWaitIdle(device);
            generateGroundPlane(groundPlaneCellSize);
        }

        // Handle quick unload (no loading overlay needed)
        if (!pendingBenchmarkLoad.empty() && pendingBenchmarkLoad == "__unload__") {
            pendingBenchmarkLoad.clear();
            vkDeviceWaitIdle(device);
            cleanupBenchmarkMesh();
            renderBenchmarkMesh = false;
        }
    }

    vkWaitForFences(device, 1, &inFlightFences[currentFrame],
                    VK_TRUE, UINT64_MAX);

    // Recreate query pool if invoc-stats toggle changed
    // Wait for all in-flight fences (not vkDeviceWaitIdle — avoids disturbing semaphore state)
    if (showGPUInvocStats != invocStatsActive) {
        vkWaitForFences(device, static_cast<uint32_t>(inFlightFences.size()),
                        inFlightFences.data(), VK_TRUE, UINT64_MAX);
        vkDestroyQueryPool(device, statsQueryPool, nullptr);
        VkQueryPoolCreateInfo qpInfo{};
        qpInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qpInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        qpInfo.queryCount = STATS_QUERY_COUNT;
        qpInfo.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT;
        if (showGPUInvocStats)
            qpInfo.pipelineStatistics |=
                VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT |
                VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT;
        vkCreateQueryPool(device, &qpInfo, nullptr, &statsQueryPool);
        // Host-side reset so the new pool is in a valid state before first use
        vkResetQueryPool(device, statsQueryPool, 0, STATS_QUERY_COUNT);
        invocStatsActive = showGPUInvocStats;
        if (!invocStatsActive) {
            gpuTaskShaderInvocations = 0;
            gpuMeshShaderInvocations = 0;
        }
    }

    // Read back pipeline statistics from the previous frame on this slot
    if (invocStatsActive) {
        uint64_t stats[3] = {};
        VkResult qr = vkGetQueryPoolResults(
            device, statsQueryPool, currentFrame, 1,
            sizeof(stats), stats, sizeof(stats),
            VK_QUERY_RESULT_64_BIT);
        if (qr == VK_SUCCESS) {
            gpuRenderedTriangles     = stats[0];
            gpuTaskShaderInvocations = stats[1];
            gpuMeshShaderInvocations = stats[2];
        }
    } else {
        uint64_t clipping = 0;
        VkResult qr = vkGetQueryPoolResults(
            device, statsQueryPool, currentFrame, 1,
            sizeof(uint64_t), &clipping, sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT);
        if (qr == VK_SUCCESS)
            gpuRenderedTriangles = clipping;
    }

    VkResult result = vkAcquireNextImageKHR(
        device, swapChain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE, &currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    frameStarted = true;
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    frameDrawCalls = 0;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.392f, 0.584f, 0.929f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Reset query pool outside render pass (required), begin inside (begin and end must match scope)
    vkCmdResetQueryPool(cmd, statsQueryPool, currentFrame, 1);

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBeginQuery(cmd, statsQueryPool, currentFrame, 0);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             pipelineLayout, 0, 1,
                             &sceneDescriptorSets[currentFrame],
                             0, nullptr);

    if (heMeshUploaded) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 1, 1,
                                 &heDescriptorSet,
                                 0, nullptr);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             pipelineLayout, 2, 1,
                             &perObjectDescriptorSet,
                             0, nullptr);

    // Update view UBO from current camera state
    {
        float aspect = static_cast<float>(swapChainExtent.width) /
                       static_cast<float>(swapChainExtent.height);

        ViewUBO viewData{};
        viewData.view           = activeCamera->getViewMatrix();
        viewData.projection     = activeCamera->getProjectionMatrix(aspect);
        viewData.cameraPosition = glm::vec4(activeCamera->getPosition(), 1.0f);
        viewData.nearPlane      = activeCamera->nearPlane;
        viewData.farPlane       = activeCamera->farPlane;
        memcpy(viewUBOMapped[currentFrame], &viewData, sizeof(ViewUBO));
    }

    // Update shading UBO with current lighting config
    GlobalShadingUBO shadingData{};
    shadingData.lightPosition = glm::vec4(lightPosition, 0.0f);
    shadingData.ambient = glm::vec4(ambientColor, ambientIntensity);
    shadingData.diffuse = diffuseIntensity;
    shadingData.specular = specularIntensity;
    shadingData.shininess = shininess;
    shadingData.metalF0 = metalF0;
    shadingData.envReflection = envReflection;
    shadingData.metalDiffuse = metalDiffuse;
    memcpy(shadingUBOMapped[currentFrame], &shadingData, sizeof(GlobalShadingUBO));

    // Per-frame animation update
    if (skeletonLoaded && animationPlaying && !animations.empty()) {
        animationTime += lastDeltaTime * animationSpeed;
        if (animationTime > animations[0].duration) {
            animationTime = std::fmod(animationTime, animations[0].duration);
        }
        GltfLoader::updateSkeleton(animations[0], animationTime, skeleton);
        std::vector<glm::mat4> boneMatrices;
        GltfLoader::computeBoneMatrices(skeleton, boneMatrices);
        boneMatricesBuffer.update(boneMatrices.data(),
                                  boneMatrices.size() * sizeof(glm::mat4));
    }

    // Update ResurfacingUBO with current state
    {
        ResurfacingUBO resurfData{};
        resurfData.elementType      = elementType;
        resurfData.userScaling      = userScaling;
        resurfData.resolutionM      = resolutionM;
        resurfData.resolutionN      = resolutionN;
        resurfData.torusMajorR      = torusMajorR;
        resurfData.torusMinorR      = torusMinorR;
        resurfData.sphereRadius     = sphereRadius;
        resurfData.doLod            = enableLod ? 1u : 0u;
        resurfData.lodFactor        = lodFactor;
        resurfData.doCulling        = (enableFrustumCulling ? 1u : 0u) | (enableBackfaceCulling ? 2u : 0u);
        resurfData.cullingThreshold = cullingThreshold;
        resurfData.doSkinning            = (skeletonLoaded && doSkinning) ? 1u : 0u;
        resurfData.hasElementTypeTexture = (useElementTypeTexture && elementTypeTextureLoaded) ? 1u : 0u;
        resurfData.hasAOTexture          = (useAOTexture && aoTextureLoaded) ? 1u : 0u;
        resurfData.hasMaskTexture        = (useMaskTexture && maskTextureLoaded) ? 1u : 0u;
        memcpy(resurfacingUBOMapped, &resurfData, sizeof(ResurfacingUBO));
    }

    PushConstants pushConstants{};

    pushConstants.model = thirdPersonMode ? player.getModelMatrix() : glm::mat4(1.0f);
    pushConstants.nbFaces = heNbFaces;
    pushConstants.nbVertices = heNbVertices;
    pushConstants.elementType = elementType;
    pushConstants.userScaling = userScaling;
    pushConstants.torusMajorR = torusMajorR;
    pushConstants.torusMinorR = torusMinorR;
    pushConstants.sphereRadius = sphereRadius;
    pushConstants.resolutionM = resolutionM;
    pushConstants.resolutionN = resolutionN;
    pushConstants.debugMode = debugMode;
    pushConstants.enableCulling = (enableFrustumCulling ? 1u : 0u) | (enableBackfaceCulling ? 2u : 0u)
                                | ((useMaskTexture && maskTextureLoaded) ? 4u : 0u);
    pushConstants.cullingThreshold = cullingThreshold;
    pushConstants.enableLod = enableLod ? 1u : 0u;
    pushConstants.lodFactor = lodFactor;
    pushConstants.chainmailMode = chainmailMode ? 1u : 0u;
    pushConstants.chainmailTiltAngle = chainmailTiltAngle;

    vkCmdPushConstants(cmd, pipelineLayout,
                        VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(PushConstants), &pushConstants);
    if (renderResurfacing && !renderPebbles) {
        if (heMeshUploaded) {
            // CPU pre-cull: build compact visible element index list
            cachedVisibleIndices.clear();
            cachedTotalElements = 0;

            bool doMaskCull = useMaskTexture && maskTextureLoaded && !cpuMaskPixels.empty();
            bool doCulling   = enableFrustumCulling || enableBackfaceCulling;

            glm::mat4 mvp;
            if (doCulling) {
                float aspect = static_cast<float>(swapChainExtent.width) /
                               static_cast<float>(swapChainExtent.height);
                glm::mat4 model = thirdPersonMode ? player.getModelMatrix() : glm::mat4(1.0f);
                mvp = activeCamera->getProjectionMatrix(aspect) *
                      activeCamera->getViewMatrix() * model;
            }

            auto isMasked = [&](glm::vec2 uv) -> bool {
                if (!doMaskCull) return false;
                uint32_t x = static_cast<uint32_t>(uv.x * cpuMaskWidth)  % cpuMaskWidth;
                uint32_t y = static_cast<uint32_t>(uv.y * cpuMaskHeight) % cpuMaskHeight;
                return cpuMaskPixels[y * cpuMaskWidth + x] < 128;
            };

            auto isVisible = [&](glm::vec3 pos, glm::vec3 normal, float area) -> bool {
                if (!doCulling) return true;
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
                    glm::vec3 viewDir = glm::normalize(activeCamera->getPosition() - pos);
                    if (glm::dot(viewDir, normal) <= cullingThreshold) return false;
                }
                return true;
            };

            uint32_t totalElements = heNbFaces + heNbVertices;
            cachedVisibleIndices.reserve(std::min(totalElements, VISIBLE_INDICES_MAX));

            auto cullStart = std::chrono::high_resolution_clock::now();

            for (uint32_t i = 0; i < heNbFaces; i++) {
                if (isMasked(cpuFaceUVs[i])) continue;
                cachedTotalElements++;
                if (isVisible(cpuFaceCenters[i], cpuFaceNormals[i], cpuFaceAreas[i]))
                    if (cachedVisibleIndices.size() < VISIBLE_INDICES_MAX)
                        cachedVisibleIndices.push_back(i);
            }
            for (uint32_t i = 0; i < heNbVertices; i++) {
                if (isMasked(cpuVertexUVs[i])) continue;
                cachedTotalElements++;
                if (isVisible(cpuVertexPositions[i], cpuVertexNormals[i], cpuVertexFaceAreas[i]))
                    if (cachedVisibleIndices.size() < VISIBLE_INDICES_MAX)
                        cachedVisibleIndices.push_back(heNbFaces + i);
            }

            cpuCullTimeMs = std::chrono::duration<float, std::milli>(
                std::chrono::high_resolution_clock::now() - cullStart).count();

            // Upload visible indices to GPU buffer and dispatch
            uint32_t visibleCount = static_cast<uint32_t>(cachedVisibleIndices.size());

            // Compute CPU-estimated mesh shader workgroup count (LOD off, tile grid per element)
            if (!enableLod && visibleCount > 0) {
                uint32_t M = resolutionM, N = resolutionN;
                uint32_t dU = M, dV = N;
                if ((dU + 1) * (dV + 1) > 256) {
                    uint32_t maxD = static_cast<uint32_t>(std::sqrt(256.0f)) - 1;
                    dU = std::min(dU, maxD);
                    dV = std::min(dV, maxD);
                }
                if (dU * dV * 2 > 256) {
                    uint32_t maxD = static_cast<uint32_t>(std::sqrt(256.0f / 2.0f));
                    dU = std::min(dU, maxD);
                    dV = std::min(dV, maxD);
                }
                dU = std::max(dU, 2u);
                dV = std::max(dV, 2u);
                uint32_t tilesU = (M + dU - 1) / dU;
                uint32_t tilesV = (N + dV - 1) / dV;
                cachedEstMeshShaders = visibleCount * tilesU * tilesV;
            } else {
                cachedEstMeshShaders = 0;
            }

            if (visibleCount > 0) {
                memcpy(visibleIndicesMapped[currentFrame],
                       cachedVisibleIndices.data(),
                       visibleCount * sizeof(uint32_t));
                pfnCmdDrawMeshTasksEXT(cmd, visibleCount, 1, 1);
                frameDrawCalls++;
            }
        } else {
            cachedVisibleIndices.clear();
            cachedTotalElements = 0;
            cachedEstMeshShaders = 0;
            cpuCullTimeMs = 0.0f;
            pfnCmdDrawMeshTasksEXT(cmd, 1, 1, 1);
            frameDrawCalls++;
        }
    }

    // Pebble pipeline draw path
    if (renderPebbles && heMeshUploaded) {
        // Update PebbleUBO
        pebbleUBO.hasAOTexture = (useAOTexture && aoTextureLoaded) ? 1u : 0u;
        pebbleUBO.doSkinning = (skeletonLoaded && doSkinning) ? 1u : 0u;
        memcpy(pebbleUBOMapped, &pebbleUBO, sizeof(PebbleUBO));

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pebblePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 1, 1,
                                 &heDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 2, 1,
                                 &pebblePerObjectDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(PushConstants), &pushConstants);
        pfnCmdDrawMeshTasksEXT(cmd, heNbFaces, 1, 1);
        frameDrawCalls++;
    }

    // Pebble control cage overlay
    if (showControlCage && renderPebbles && heMeshUploaded) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pebbleCagePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 1, 1,
                                 &heDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 2, 1,
                                 &pebblePerObjectDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(PushConstants), &pushConstants);
        pfnCmdDrawMeshTasksEXT(cmd, heNbFaces, 1, 1);
        frameDrawCalls++;
    }

    // Ground pathway pebbles
    if (renderPathway && groundMeshActive) {
        // Update per-frame and pathway-specific fields (pebble appearance is controlled independently via UI)
        groundPebbleUBO.usePathway       = fogOfWar ? 1u : 0u;
        groundPebbleUBO.playerWorldPos   = player.position;
        groundPebbleUBO.pad1             = 0.0f;
        groundPebbleUBO.playerForward    = playerForwardDir();
        groundPebbleUBO.pad2             = 0.0f;
        groundPebbleUBO.pathwayRadius    = pathwayRadius;
        groundPebbleUBO.pathwayBackScale = pathwayBackScale;
        groundPebbleUBO.pathwayFalloff   = pathwayFalloff;
        groundPebbleUBO.time             = pebbleUBO.time;
        groundPebbleUBO.doSkinning       = 0;
        groundPebbleUBO.hasAOTexture     = 0;

        // Apply ground pebble scale to a copy for upload
        PebbleUBO groundUpload = groundPebbleUBO;
        groundUpload.extrusionAmount *= groundPebbleScale;
        memcpy(groundPebbleUBOMapped, &groundUpload, sizeof(PebbleUBO));

        // Ground plane is stationary — model matrix stays at world origin
        PushConstants groundPush = pushConstants;
        groundPush.model        = glm::mat4(1.0f);
        groundPush.nbFaces      = groundNbFaces;
        groundPush.nbVertices   = 0;
        groundPush.enableCulling &= ~4u;  // no mask texture on ground plane

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pebblePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 1, 1,
                                 &groundHeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 2, 1,
                                 &groundPebbleDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(PushConstants), &groundPush);
        pfnCmdDrawMeshTasksEXT(cmd, groundNbFaces, 1, 1);
        frameDrawCalls++;
    }

    // Ground plane wireframe overlay
    if (showGroundMesh && groundMeshActive) {
        PushConstants gp = pushConstants;
        gp.model    = glm::mat4(1.0f);
        gp.nbFaces  = groundNbFaces;
        gp.nbVertices = 0;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, baseMeshPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 1, 1,
                                 &groundHeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 2, 1,
                                 &groundPebbleDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(PushConstants), &gp);
        pfnCmdDrawMeshTasksEXT(cmd, groundNbFaces, 1, 1);
        frameDrawCalls++;
    }

    // Dual-mesh: render secondary mesh as solid base under coat
    if (renderResurfacing && dualMeshActive && heMeshUploaded) {
        PushConstants basePush = pushConstants;
        basePush.model = pushConstants.model;  // inherit player transform
        basePush.nbFaces = secondaryHeNbFaces;
        basePush.nbVertices = secondaryHeNbVertices;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, baseMeshSolidPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 1, 1,
                                 &secondaryHeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 2, 1,
                                 &secondaryPerObjectDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(PushConstants), &basePush);
        pfnCmdDrawMeshTasksEXT(cmd, secondaryHeNbFaces, 1, 1);
        frameDrawCalls++;
    }

    // Base mesh overlay (0=off, 1=wireframe, 2=solid, 3=both)
    if (baseMeshMode > 0 && heMeshUploaded) {
        auto drawBaseMesh = [&](VkPipeline pipeline) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     pipelineLayout, 0, 1,
                                     &sceneDescriptorSets[currentFrame],
                                     0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     pipelineLayout, 1, 1,
                                     &heDescriptorSet,
                                     0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     pipelineLayout, 2, 1,
                                     &perObjectDescriptorSet,
                                     0, nullptr);
            vkCmdPushConstants(cmd, pipelineLayout,
                                VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                                VK_SHADER_STAGE_FRAGMENT_BIT,
                                0, sizeof(PushConstants), &pushConstants);
            pfnCmdDrawMeshTasksEXT(cmd, heNbFaces, 1, 1);
            frameDrawCalls++;
        };

        if (baseMeshMode == 5) {
            pushConstants.debugMode = 101;  // signal skin texture to frag shader
            drawBaseMesh(baseMeshSolidPipeline);
            pushConstants.debugMode = debugMode;  // restore
        } else if (baseMeshMode == 4) {
            pushConstants.debugMode = 100;  // signal mask preview to frag shader
            drawBaseMesh(baseMeshSolidPipeline);
            pushConstants.debugMode = debugMode;  // restore
        } else if (baseMeshMode == 2 || baseMeshMode == 3) {
            drawBaseMesh(baseMeshSolidPipeline);
        }
        if (baseMeshMode == 1 || baseMeshMode == 3)
            drawBaseMesh(baseMeshPipeline);
    }

    // Benchmark mesh (traditional vertex pipeline)
    if (renderBenchmarkMesh && benchmarkMeshLoaded) {
        float aspect = static_cast<float>(swapChainExtent.width) /
                       static_cast<float>(swapChainExtent.height);
        BenchmarkPushConstants benchPush{};
        benchPush.model = glm::mat4(1.0f);
        benchPush.view = activeCamera->getViewMatrix();
        benchPush.projection = activeCamera->getProjectionMatrix(aspect);
        benchPush.debugMode = debugMode;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, benchmarkPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 benchmarkPipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdPushConstants(cmd, benchmarkPipelineLayout,
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(BenchmarkPushConstants), &benchPush);

        VkBuffer vertexBuffers[] = { benchmarkVertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, benchmarkIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, benchmarkIndexCount, 1, 0, 0, 0);
        frameDrawCalls++;
    }

    // End pipeline statistics query before ImGui (exclude UI triangles, same subpass as begin)
    vkCmdEndQuery(cmd, statsQueryPool, currentFrame);

    // Draw ImGui on top
    renderImGui(cmd);

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

void Renderer::endFrame() {
    if (!frameStarted) return;

    recordCommandBuffer(commandBuffers[currentFrame], currentImageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                      inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &currentImageIndex;

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || pendingSwapChainRecreation) {
        pendingSwapChainRecreation = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    frameStarted = false;
}

void Renderer::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window.getHandle(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window.getHandle(), &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createFramebuffers();

    std::cout << "Swap chain recreated: " << width << "x" << height << std::endl;
}

void Renderer::waitIdle() {
    vkDeviceWaitIdle(device);
}

// ============================================================================
// Procedural Mesh Export
// ============================================================================

void Renderer::exportProceduralMesh(const std::string& filepath, int mode) {
    if (!heMeshUploaded) {
        throw std::runtime_error("No mesh loaded");
    }

    vkDeviceWaitIdle(device);

    // Ensure compute pipelines are created (lazy init)
    createExportComputePipelines();

    // --- 1. Calculate total counts and build offset buffer ---
    uint32_t totalVerts = 0, totalTris = 0;
    std::vector<ExportElementOffset> offsets;

    if (mode == 0) {
        // Parametric: fixed geometry per element
        uint32_t M = resolutionM, N = resolutionN;
        uint32_t vertsPerElement = (M + 1) * (N + 1);
        uint32_t trisPerElement = M * N * 2;
        uint32_t numElements = heNbFaces + heNbVertices;

        offsets.resize(numElements);
        for (uint32_t i = 0; i < numElements; i++) {
            offsets[i].vertexOffset = totalVerts;
            offsets[i].triangleOffset = totalTris;
            offsets[i].isVertex = (i >= heNbFaces) ? 1 : 0;
            offsets[i].faceId = (i >= heNbFaces) ? (i - heNbFaces) : i;
            totalVerts += vertsPerElement;
            totalTris += trisPerElement;
        }
    } else {
        throw std::runtime_error("Pebble export not yet implemented");
    }

    std::cout << "Export: " << totalVerts << " vertices, "
              << totalTris << " triangles" << std::endl;

    // --- 2. Allocate export buffers ---
    MeshExportBuffers exportBufs;
    exportBufs.allocate(device, physicalDevice, totalVerts, totalTris, offsets);

    // --- 3. Create on-demand descriptor pool + set ---
    VkDescriptorPool exportPool = VK_NULL_HANDLE;
    VkDescriptorSet exportSet = VK_NULL_HANDLE;

    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 5;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &exportPool) != VK_SUCCESS) {
            exportBufs.destroy();
            throw std::runtime_error("Failed to create export descriptor pool");
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = exportPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &exportOutputSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &exportSet) != VK_SUCCESS) {
            vkDestroyDescriptorPool(device, exportPool, nullptr);
            exportBufs.destroy();
            throw std::runtime_error("Failed to allocate export descriptor set");
        }

        // Write descriptor set
        std::array<VkDescriptorBufferInfo, 5> bufInfos{};
        bufInfos[0] = {exportBufs.positions.getBuffer(), 0, exportBufs.positions.getSize()};
        bufInfos[1] = {exportBufs.normals.getBuffer(), 0, exportBufs.normals.getSize()};
        bufInfos[2] = {exportBufs.uvs.getBuffer(), 0, exportBufs.uvs.getSize()};
        bufInfos[3] = {exportBufs.indices.getBuffer(), 0, exportBufs.indices.getSize()};
        bufInfos[4] = {exportBufs.offsets.getBuffer(), 0, exportBufs.offsets.getSize()};

        std::array<VkWriteDescriptorSet, 5> writes{};
        for (uint32_t i = 0; i < 5; i++) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = exportSet;
            writes[i].dstBinding = i;
            writes[i].dstArrayElement = 0;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].descriptorCount = 1;
            writes[i].pBufferInfo = &bufInfos[i];
        }

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }

    // --- 4. Record and submit compute command buffer ---
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkPipeline pipeline = (mode == 0) ? parametricExportPipeline : parametricExportPipeline; // TODO: pebble
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    // Bind descriptor sets 0-3
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout, 0, 1,
                            &sceneDescriptorSets[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout, 1, 1,
                            &heDescriptorSet, 0, nullptr);

    VkDescriptorSet perObjSet = (mode == 0) ? perObjectDescriptorSet
                                            : pebblePerObjectDescriptorSet;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout, 2, 1,
                            &perObjSet, 0, nullptr);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout, 3, 1,
                            &exportSet, 0, nullptr);

    // Push constants
    PushConstants pc{};
    pc.model = glm::mat4(1.0f);
    pc.nbFaces = heNbFaces;
    pc.nbVertices = heNbVertices;
    pc.elementType = elementType;
    pc.userScaling = userScaling;
    pc.torusMajorR = torusMajorR;
    pc.torusMinorR = torusMinorR;
    pc.sphereRadius = sphereRadius;
    pc.resolutionM = resolutionM;
    pc.resolutionN = resolutionN;
    pc.debugMode = 0;
    pc.enableCulling = 0;       // No culling for export
    pc.cullingThreshold = 0.0f;
    pc.enableLod = 0;           // No LOD for export
    pc.lodFactor = 1.0f;
    pc.chainmailMode = chainmailMode ? 1 : 0;
    pc.chainmailTiltAngle = chainmailTiltAngle;

    vkCmdPushConstants(cmd, computePipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(PushConstants), &pc);

    uint32_t numWorkgroups = static_cast<uint32_t>(offsets.size());
    vkCmdDispatch(cmd, numWorkgroups, 1, 1);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);

    // --- 5. Read back and write OBJ ---
    void* posData = nullptr;
    void* normData = nullptr;
    void* uvData = nullptr;
    void* idxData = nullptr;

    vkMapMemory(device, exportBufs.positions.getMemory(), 0,
                totalVerts * sizeof(glm::vec4), 0, &posData);
    vkMapMemory(device, exportBufs.normals.getMemory(), 0,
                totalVerts * sizeof(glm::vec4), 0, &normData);
    vkMapMemory(device, exportBufs.uvs.getMemory(), 0,
                totalVerts * sizeof(glm::vec2), 0, &uvData);
    vkMapMemory(device, exportBufs.indices.getMemory(), 0,
                totalTris * 3 * sizeof(uint32_t), 0, &idxData);

    ObjWriter::write(filepath,
                     static_cast<const glm::vec4*>(posData),
                     static_cast<const glm::vec4*>(normData),
                     static_cast<const glm::vec2*>(uvData),
                     static_cast<const uint32_t*>(idxData),
                     totalVerts, totalTris);

    vkUnmapMemory(device, exportBufs.positions.getMemory());
    vkUnmapMemory(device, exportBufs.normals.getMemory());
    vkUnmapMemory(device, exportBufs.uvs.getMemory());
    vkUnmapMemory(device, exportBufs.indices.getMemory());

    // --- 6. Append base mesh if visible ---
    if (baseMeshMode > 0 && !loadedMeshPath.empty()) {
        NGonMesh baseMesh = ObjLoader::load(loadedMeshPath);
        // OBJ indices are 1-based; offset by the procedural vertex count
        ObjWriter::appendMesh(filepath, baseMesh, totalVerts + 1);
    }

    // --- 7. Cleanup ---
    vkDestroyDescriptorPool(device, exportPool, nullptr);
    exportBufs.destroy();

    std::cout << "Export complete: " << filepath << std::endl;
}
