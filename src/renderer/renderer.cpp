#include "renderer/renderer.h"
#include "loaders/GltfLoader.h"
#include "core/window.h"
#include <stdexcept>
#include <iostream>
#include <array>
#include <cstring>
#include <cmath>

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
    createDescriptorSetLayouts();
    createPipelineLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createGraphicsPipeline();
    createSamplers();
    initImGui();
}

Renderer::~Renderer() {
    cleanupImGui();
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
    }

    if (resurfacingUBOBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, resurfacingUBOBuffer, nullptr);
        vkFreeMemory(device, resurfacingUBOMemory, nullptr);
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
    // Process deferred mesh load between frames (before recording command buffers)
    // to avoid updating descriptor sets while a command buffer is being recorded
    if (!pendingMeshLoad.empty()) {
        std::string path = std::move(pendingMeshLoad);
        pendingMeshLoad.clear();
        loadMesh(path);

        // Re-apply preset state that loadMesh() resets (cleanupMeshSkeleton clears these)
        if (pendingPreset) {
            doSkinning = pendingPreset->doSkinning;
            animationPlaying = pendingPreset->animationPlaying;
            animationSpeed = pendingPreset->animationSpeed;
            baseMeshMode = pendingPreset->baseMeshMode;
            pendingPreset = nullptr;
        }
    }

    vkWaitForFences(device, 1, &inFlightFences[currentFrame],
                    VK_TRUE, UINT64_MAX);

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

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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
    uint32_t totalTasks = heMeshUploaded
        ? (heNbFaces + heNbVertices)
        : 1;
    pfnCmdDrawMeshTasksEXT(cmd, totalTasks, 1, 1);

    // Dual-mesh: render secondary mesh as solid base under coat
    if (dualMeshActive && heMeshUploaded) {
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
