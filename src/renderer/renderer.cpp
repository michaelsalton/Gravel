#include "renderer/renderer.h"
#include "renderer/MeshExport.h"
#include "loaders/ObjWriter.h"
#include "loaders/ObjLoader.h"
#include "loaders/GltfLoader.h"
#include "core/window.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <stb_image.h>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <array>
#include <cstring>
#include <cmath>
#include <chrono>

// *** Michael Salton ***

// Constructor: brings up the whole renderer in dependency order — Vulkan core,
// swapchain/attachments, the mesh-shader pipelines and descriptor resources,
// then scene assets (ground plane, scale LUT, skybox, default mesh) and ImGui.
Renderer::Renderer(Window& window) : window(window) { // Keep a reference to the window (surface/extent source)
    createInstance();                            // Vulkan instance (+ validation layers)
    setupDebugMessenger();                       // Validation-layer debug callback
    createSurface();                             // Window surface to present to
    pickPhysicalDevice();                        // Choose a GPU with mesh-shader support
    createLogicalDevice();                       // Logical device + queues (mesh shader features enabled)
    loadMeshShaderFunctions();                   // Resolve vkCmdDrawMeshTasksEXT
    createCommandPool();                         // Pool for command buffers
    createSwapChain();                           // Swapchain images
    createImageViews();                          // Views over them
    createDepthResources();                      // Depth buffer
    createMsaaColorResources();                  // MSAA color target (if enabled)
    createRenderPass();                          // Render pass (color + depth, optional resolve)
    createFramebuffers();                        // One framebuffer per swapchain image
    createCommandBuffers();                      // Per-frame command buffers
    createSyncObjects();                         // Semaphores + fences

    // Pipeline statistics query pool for real-time triangle counting
    {
        VkQueryPoolCreateInfo queryPoolInfo{};   // Query pool for the on-screen GPU stats
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        queryPoolInfo.queryCount = STATS_QUERY_COUNT; // One slot per frame in flight
        queryPoolInfo.pipelineStatistics =       // Start with just the clipped-primitive (triangle) counter
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT;
        if (vkCreateQueryPool(device, &queryPoolInfo, nullptr, &statsQueryPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create statistics query pool!");
        }
        vkResetQueryPool(device, statsQueryPool, 0, STATS_QUERY_COUNT); // Host-reset into a valid initial state
    }

    createDescriptorSetLayouts();                // Scene / HalfEdge / PerObject set layouts
    createPipelineLayout();                      // Pipeline layout (3 sets + push constants)
    createUniformBuffers();                      // View/Shading/Resurfacing/Pebble UBOs + pre-cull/stats SSBOs
    createDescriptorPool();                      // Pool sized for all the sets
    createDescriptorSets();                      // Allocate + write the scene/per-object sets
    createGraphicsPipeline();                    // The task+mesh+frag resurfacing pipeline
    createBenchmarkPipeline();                   // Traditional vertex pipeline (for comparison)
    createSamplers();                            // Linear + nearest samplers
    generateGroundPlane(groundPlaneCellSize);    // Build the procedural ground plane
    loadScaleLut();                              // Dragon-scale B-spline control cage
    scanSkyboxes();                              // Discover available skyboxes
    if (!skyboxPaths.empty()) {                  // If any skyboxes were found...
        selectedSkybox = 0;                      // Default to the first
        // Default to ludwikowice if available
        for (int i = 0; i < static_cast<int>(skyboxNames.size()); i++) { // ...but prefer "ludwikowice" if present
            if (skyboxNames[i].find("ludwikowice") != std::string::npos) {
                selectedSkybox = i;
                break;
            }
        }
        loadSkybox(skyboxPaths[selectedSkybox]); // Load the chosen skybox HDR
    }
    precomputeProxyParams();                     // Precompute per-element-type proxy PBR params
    scanAssetMeshes();                           // Discover loadable base meshes
    if (selectedMesh >= 0 && selectedMesh < static_cast<int>(assetMeshPaths.size())) // If a default mesh is selected...
        pendingMeshLoad = assetMeshPaths[selectedMesh]; // ...queue it to load on the first frame
    initImGui();                                 // Initialize the ImGui UI backend
}

// *** ************ ***

// *** AI Generated ***

Renderer::~Renderer() {
    vkDeviceWaitIdle(device);
    cleanupImGui();
    if (statsQueryPool != VK_NULL_HANDLE)
        vkDestroyQueryPool(device, statsQueryPool, nullptr);
    cleanupExportPipelines();
    cleanupBenchmarkMesh();
    cleanupGroundMesh();
    cleanupSecondaryMesh();
    cleanupMeshSkeleton();
    cleanupMeshTextures();
    cleanupScaleLut();
    cleanupGrwmPreprocess();
    if (benchmarkPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device, benchmarkPipeline, nullptr);
    if (benchmarkPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, benchmarkPipelineLayout, nullptr);
    vkDestroyPipeline(device, pebbleCagePipeline, nullptr);
    vkDestroyPipeline(device, pebblePipeline, nullptr);
    vkDestroyPipeline(device, baseMeshSolidPipeline, nullptr);
    vkDestroyPipeline(device, baseMeshPipeline, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);

    // Skybox cleanup
    cleanupSkyboxTexture();
    if (skyboxSampler != VK_NULL_HANDLE) vkDestroySampler(device, skyboxSampler, nullptr);
    if (skyboxPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, skyboxPipeline, nullptr);
    if (skyboxPipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, skyboxPipelineLayout, nullptr);
    if (skyboxDescriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, skyboxDescriptorSetLayout, nullptr);
    if (skyboxUBOBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, skyboxUBOBuffer, nullptr);
        vkFreeMemory(device, skyboxUBOMemory, nullptr);
    }

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

    if (secondaryResurfacingUBOBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, secondaryResurfacingUBOBuffer, nullptr);
        vkFreeMemory(device, secondaryResurfacingUBOMemory, nullptr);
    }

    if (pebbleUBOBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, pebbleUBOBuffer, nullptr);
        vkFreeMemory(device, pebbleUBOMemory, nullptr);
    }

    if (proxyFlagBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, proxyFlagBuffer, nullptr);
        vkFreeMemory(device, proxyFlagMemory, nullptr);
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (i < static_cast<int>(elementStatsBuffers.size()) && elementStatsBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, elementStatsBuffers[i], nullptr);
            vkFreeMemory(device, elementStatsMemory[i], nullptr);
        }
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

// *** ************ ***

// *** Michael Salton ***

// Start a frame: process any deferred/heavy work (mesh & benchmark loads, export,
// ground regen, skybox/coat/GRWM loads) — heavy ops wait two frames so a loading
// overlay is visible first — then read back the previous frame's GPU stats, wait
// on this slot's fence, and acquire the next swapchain image. Sets frameStarted.
void Renderer::beginFrame() {
    // Check if any heavy operation is pending — show loading overlay first frame,
    // then do the actual work on the next frame
    bool hasPendingWork = !pendingMeshLoad.empty() || // Is a heavy op queued? (mesh load,
                          (!pendingBenchmarkLoad.empty() && pendingBenchmarkLoad != "__unload__") || // benchmark load,
                          pendingExport;          // or export)

    if (hasPendingWork && !loadingActive) {
        // First frame: just set loading flag, let this frame render the overlay
        loadingActive = true;                    // Enter "loading" state
        loadingFrameCount = 0;                   // Reset the overlay-visible frame counter
        if (!pendingMeshLoad.empty())            // Pick an overlay message for the queued op
            loadingMessage = "Loading mesh...";
        else if (!pendingBenchmarkLoad.empty())
            loadingMessage = "Loading benchmark mesh...";
        else if (pendingExport)
            loadingMessage = "Exporting mesh...";
        loadingStartTime = static_cast<float>(glfwGetTime()); // Stamp the start time
        // Don't process the work yet — fall through to render a frame with the overlay
    } else if (loadingActive && loadingFrameCount < 2) {
        // Wait for overlay to be visible on screen (need 2 frames: render + present)
        loadingFrameCount++;                     // Count overlay frames (need render + present)
        // Fall through to render another frame with the overlay
    } else if (loadingActive) {
        // Overlay has been shown for 2 frames, now do the actual work
        if (pendingGroundRegenerate) {           // Deferred ground-plane regen
            pendingGroundRegenerate = false;
            vkDeviceWaitIdle(device);            // Safe to mutate GPU resources only when idle
            generateGroundPlane(groundPlaneCellSize);
        }

        if (!pendingMeshLoad.empty()) {          // Deferred mesh load
            std::string path = std::move(pendingMeshLoad); // Take and clear the request
            pendingMeshLoad.clear();
            loadMesh(path);                      // Do the actual (heavy) load

            if (pendingPreset) {                 // A UI preset may accompany the load
                doSkinning = pendingPreset->doSkinning;       // Apply preset toggles
                animationPlaying = pendingPreset->animationPlaying;
                animationSpeed = pendingPreset->animationSpeed;
                baseMeshMode = pendingPreset->baseMeshMode;
                if (pendingPreset->chainmailMode) {           // Chainmail preset
                    applyPresetChainMail();
                }
                if (pendingPreset->enableDragonCoat && dragonCoatAvailable) { // Coat preset
                    dragonCoatEnabled = true;
                    loadSecondaryMesh(dragonCoatPath);
                }
                if (pendingPreset->applyDragonScales) {       // Dragon-scale preset
                    applyPresetDragonScales();
                    dragonBaseMeshMode = 2;  // Solid
                }
                pendingPreset = nullptr;         // Preset consumed
            }
        }

        if (!pendingBenchmarkLoad.empty()) {     // Deferred benchmark mesh load/unload
            std::string path = std::move(pendingBenchmarkLoad);
            pendingBenchmarkLoad.clear();
            if (path == "__unload__") {          // Sentinel = unload the benchmark mesh
                vkDeviceWaitIdle(device);
                cleanupBenchmarkMesh();
                renderBenchmarkMesh = false;
            } else {                             // Otherwise load the given benchmark mesh
                loadBenchmarkMesh(path);
            }
        }

        if (pendingExport) {                     // Deferred procedural-mesh export
            pendingExport = false;
            try {
                auto pos = exportFilePath.find_last_of("/\\"); // Ensure the output directory exists
                if (pos != std::string::npos) {
                    std::filesystem::create_directories(exportFilePath.substr(0, pos));
                }
                exportProceduralMesh(exportFilePath, exportMode); // Bake + write the mesh
                lastExportStatus = "Exported: " + exportFilePath; // UI status
            } catch (const std::exception& e) {  // Report failures to the UI rather than crashing
                lastExportStatus = std::string("Export failed: ") + e.what();
            }
        }

        loadingDuration = static_cast<float>(glfwGetTime()) - loadingStartTime; // Record how long it took
        loadingActive = false;                   // Leave loading state
        loadingDone = true;                      // Trigger the "done" UI flash
        loadingDoneTime = static_cast<float>(glfwGetTime());
    } else {
        // Deferred GRWM buffer load (after pipeline run completes)
        if (grwmPendingLoad) {                   // GRWM output becomes available after its run finishes
            grwmPendingLoad = false;
            vkDeviceWaitIdle(device);
            loadGrwmPreprocess(loadedMeshPath);  // Load curvature/feature/slot data
            writeGrwmDescriptors(heDescriptorSet); // Bind it
            grwmStatus = preprocessLoaded ? "Loaded successfully" : "Failed to load output"; // UI status
        }

        // Deferred dragon coat load/unload
        if (pendingCoatLoad) {                   // Toggle: load the dragon coat
            pendingCoatLoad = false;
            vkDeviceWaitIdle(device);
            loadSecondaryMesh(dragonCoatPath);
        }
        if (pendingCoatUnload) {                 // Toggle: unload the dragon coat
            pendingCoatUnload = false;
            vkDeviceWaitIdle(device);
            cleanupSecondaryMesh();
        }

        // Deferred skybox load
        if (!pendingSkyboxLoad.empty()) {        // UI picked a different skybox
            std::string path = std::move(pendingSkyboxLoad);
            pendingSkyboxLoad.clear();
            loadSkybox(path);
        }

        // No loading — process lightweight deferred ops
        if (pendingGroundRegenerate) {           // Ground regen without the loading overlay
            pendingGroundRegenerate = false;
            vkDeviceWaitIdle(device);
            generateGroundPlane(groundPlaneCellSize);
        }

        // Handle quick unload (no loading overlay needed)
        if (!pendingBenchmarkLoad.empty() && pendingBenchmarkLoad == "__unload__") { // Cheap benchmark unload
            pendingBenchmarkLoad.clear();
            vkDeviceWaitIdle(device);
            cleanupBenchmarkMesh();
            renderBenchmarkMesh = false;
        }
    }

    vkWaitForFences(device, 1, &inFlightFences[currentFrame], // Wait until this frame slot's prior work finished
                    VK_TRUE, UINT64_MAX);

    // Recreate query pool if invoc-stats toggle changed
    // Wait for all in-flight fences (not vkDeviceWaitIdle — avoids disturbing semaphore state)
    if (showGPUInvocStats != invocStatsActive) { // UI toggled the task/mesh invocation counters
        vkWaitForFences(device, static_cast<uint32_t>(inFlightFences.size()), // Ensure no frame is using the pool
                        inFlightFences.data(), VK_TRUE, UINT64_MAX);
        vkDestroyQueryPool(device, statsQueryPool, nullptr); // Rebuild the pool with the new statistic set
        VkQueryPoolCreateInfo qpInfo{};
        qpInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qpInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        qpInfo.queryCount = STATS_QUERY_COUNT;
        qpInfo.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT; // Always count triangles
        if (showGPUInvocStats)                   // Optionally also count task/mesh shader invocations
            qpInfo.pipelineStatistics |=
                VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT |
                VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT;
        vkCreateQueryPool(device, &qpInfo, nullptr, &statsQueryPool);
        // Host-side reset so the new pool is in a valid state before first use
        vkResetQueryPool(device, statsQueryPool, 0, STATS_QUERY_COUNT);
        invocStatsActive = showGPUInvocStats;    // Remember the new mode
        if (!invocStatsActive) {                 // Clear stale invocation counts when turning off
            gpuTaskShaderInvocations = 0;
            gpuMeshShaderInvocations = 0;
        }
    }

    // Read back pipeline statistics from the previous frame on this slot
    if (invocStatsActive) {                      // Extended mode: triangles + task + mesh invocations
        uint64_t stats[3] = {};
        VkResult qr = vkGetQueryPoolResults(     // Non-blocking read of this slot's previous-frame results
            device, statsQueryPool, currentFrame, 1,
            sizeof(stats), stats, sizeof(stats),
            VK_QUERY_RESULT_64_BIT);
        if (qr == VK_SUCCESS) {                  // Only update the UI numbers if results are ready
            gpuRenderedTriangles     = stats[0];
            gpuTaskShaderInvocations = stats[1];
            gpuMeshShaderInvocations = stats[2];
        }
    } else {                                     // Default mode: just the triangle (clipped-primitive) count
        uint64_t clipping = 0;
        VkResult qr = vkGetQueryPoolResults(
            device, statsQueryPool, currentFrame, 1,
            sizeof(uint64_t), &clipping, sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT);
        if (qr == VK_SUCCESS)
            gpuRenderedTriangles = clipping;
    }

    // Read back rendered element count from this frame's atomic counter, then reset
    gpuRenderedElements = *reinterpret_cast<uint32_t*>(elementStatsMapped[currentFrame]); // Read the atomic counter
    *reinterpret_cast<uint32_t*>(elementStatsMapped[currentFrame]) = 0; // Reset it for this frame's draws

    VkResult result = vkAcquireNextImageKHR(     // Acquire the next swapchain image to render into
        device, swapChain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame],  // Signaled when the image is ready
        VK_NULL_HANDLE, &currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {    // Window resized/invalidated...
        recreateSwapChain();                     // ...rebuild the swapchain and skip this frame
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { // Suboptimal is acceptable; other errors aren't
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &inFlightFences[currentFrame]); // Reset the fence now that we're committed to rendering
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);   // Reset this slot's command buffer for re-recording

    frameStarted = true;                         // Mark the frame open (endFrame checks this)
}

// Record one frame's command buffer: begin the render pass, draw the skybox,
// push the per-frame UBOs (view/shading/resurfacing/pebble), run the CPU pre-cull
// to build the visible-element list, then dispatch every mesh-shader draw —
// resurfacing, pebbles, control cage, ground pathway/wireframe, the dual
// (secondary) mesh, and the base-mesh overlay. frameDrawCalls tallies the draws.
void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    frameDrawCalls = 0;                          // Reset the per-frame draw-call counter

    VkCommandBufferBeginInfo beginInfo{};        // Begin recording this frame's command buffer
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};      // Configure the render pass for this frame
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex]; // Target this swapchain image's framebuffer
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};   // [0] color, [1] depth
    clearValues[0].color = {{backgroundColor.x, backgroundColor.y, backgroundColor.z, 1.0f}}; // Clear to the UI background color
    clearValues[1].depthStencil = {1.0f, 0};     // Clear depth to far

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Reset query pool outside render pass (required), begin inside (begin and end must match scope)
    vkCmdResetQueryPool(cmd, statsQueryPool, currentFrame, 1); // Reset this slot's stats query before use

    // Clear proxy face buffer before rendering (task shader writes per-face flags)
    if (enableProxy && proxyFlagBuffer != VK_NULL_HANDLE) { // Proxy mode: zero the per-face proxy buffer each frame
        vkCmdFillBuffer(cmd, proxyFlagBuffer, 0, proxyFlagSize, 0); // GPU memset to 0
        VkMemoryBarrier barrier{};               // Barrier so the fill is visible to the task/fragment shaders
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // After the transfer write...
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT; // ...before shader reads/writes
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE); // Enter the render pass

    vkCmdBeginQuery(cmd, statsQueryPool, currentFrame, 0); // Begin collecting pipeline statistics

    VkViewport viewport{};                       // Full-window viewport
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};                          // Full-window scissor
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Draw skybox first (no depth write, always behind everything)
    if (showSkybox && skyboxLoaded) {            // Only if a skybox is enabled + loaded
        float aspect = static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height); // Aspect ratio
        glm::mat4 view = activeCamera->getViewMatrix();          // Current camera view
        glm::mat4 proj = activeCamera->getProjectionMatrix(aspect); // Current projection
        glm::mat4 invVP = glm::inverse(proj * view);             // Inverse VP → reconstruct world rays in the shader

        struct { glm::mat4 invVP; float exposure; float pad[3]; } skyUBO; // Skybox UBO layout
        skyUBO.invVP = invVP;
        skyUBO.exposure = skyboxExposure;        // HDR exposure
        memcpy(skyboxUBOMapped, &skyUBO, sizeof(skyUBO)); // Upload

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline); // Bind skybox pipeline
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 skyboxPipelineLayout, 0, 1,
                                 &skyboxDescriptorSet, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);  // fullscreen triangle // One big triangle covering the screen
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline); // Bind the resurfacing pipeline

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 0: scene (view/shading/pre-cull/stats)
                             pipelineLayout, 0, 1,
                             &sceneDescriptorSets[currentFrame],
                             0, nullptr);

    if (heMeshUploaded) {                         // Set 1: half-edge data (only if a mesh is loaded)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipelineLayout, 1, 1,
                                 &heDescriptorSet,
                                 0, nullptr);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 2: per-object (config UBO, textures, ...)
                             pipelineLayout, 2, 1,
                             &perObjectDescriptorSet,
                             0, nullptr);

    // Update view UBO from current camera state
    {
        float aspect = static_cast<float>(swapChainExtent.width) / // Aspect for the projection matrix
                       static_cast<float>(swapChainExtent.height);

        ViewUBO viewData{};                      // Fill the camera UBO from the active camera
        viewData.view           = activeCamera->getViewMatrix();
        viewData.projection     = activeCamera->getProjectionMatrix(aspect);
        viewData.cameraPosition = glm::vec4(activeCamera->getPosition(), 1.0f);
        viewData.nearPlane      = activeCamera->nearPlane;
        viewData.farPlane       = activeCamera->farPlane;
        memcpy(viewUBOMapped[currentFrame], &viewData, sizeof(ViewUBO)); // Upload to this frame's mapped UBO
    }

    // Update shading UBO with current lighting config
    // (light + the primary/base-mesh/secondary PBR material params, copied from UI state)
    GlobalShadingUBO shadingData{};
    shadingData.lightPosition         = glm::vec4(lightPosition, 0.0f);
    shadingData.ambient               = glm::vec4(ambientColor, ambientIntensity);
    shadingData.lightIntensity        = lightIntensity;
    shadingData.roughness             = roughness;
    shadingData.metallic              = metallic;
    shadingData.ao                    = ao;
    shadingData.dielectricF0          = dielectricF0;
    shadingData.envReflection         = envReflection;
    shadingData.baseMeshRoughness     = baseMeshRoughness;
    shadingData.baseMeshMetallic      = baseMeshMetallic;
    shadingData.baseMeshAo            = baseMeshAo;
    shadingData.baseMeshDielectricF0  = baseMeshDielectricF0;
    shadingData.baseMeshEnvReflection = baseMeshEnvReflection;
    shadingData.procBaseColor              = glm::vec4(procBaseColor, 1.0f);
    shadingData.baseMeshBaseColor          = glm::vec4(baseMeshBaseColor, 1.0f);
    shadingData.baseMeshSolidRoughness     = baseMeshSolidRoughness;
    shadingData.baseMeshSolidMetallic      = baseMeshSolidMetallic;
    shadingData.baseMeshSolidAo            = baseMeshSolidAo;
    shadingData.baseMeshSolidDielectricF0  = baseMeshSolidDielectricF0;
    shadingData.baseMeshSolidEnvReflection = baseMeshSolidEnvReflection;
    shadingData.baseMeshSolidBaseColor        = glm::vec4(baseMeshSolidBaseColor, 1.0f);
    shadingData.secBaseMeshSolidRoughness     = secBaseMeshSolidRoughness;
    shadingData.secBaseMeshSolidMetallic      = secBaseMeshSolidMetallic;
    shadingData.secBaseMeshSolidAo            = secBaseMeshSolidAo;
    shadingData.secBaseMeshSolidDielectricF0  = secBaseMeshSolidDielectricF0;
    shadingData.secBaseMeshSolidEnvReflection = secBaseMeshSolidEnvReflection;
    shadingData.secBaseMeshSolidBaseColor     = glm::vec4(secBaseMeshSolidBaseColor, 1.0f);
    memcpy(shadingUBOMapped[currentFrame], &shadingData, sizeof(GlobalShadingUBO));

    // Per-frame animation update
    if (skeletonLoaded && animationPlaying && !animations.empty()) { // Only when a clip is playing
        animationTime += lastDeltaTime * animationSpeed; // Advance the clock by dt × speed
        if (animationTime > animations[0].duration) {    // Loop the clip
            animationTime = std::fmod(animationTime, animations[0].duration);
        }
        GltfLoader::updateSkeleton(animations[0], animationTime, skeleton); // Pose the skeleton at this time
        std::vector<glm::mat4> boneMatrices;             // Recompute the bone matrices...
        GltfLoader::computeBoneMatrices(skeleton, boneMatrices);
        boneMatricesBuffer.update(boneMatrices.data(),   // ...and upload them for skinning
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
        // Dragon scale LUT fields (set by loadScaleLut, normalPerturbation from UI)
        resurfData.Nx                 = scaleLutNx;
        resurfData.Ny                 = scaleLutNy;
        resurfData.normalPerturbation = normalPerturbation;
        resurfData.minLutExtent       = scaleLutMinExtent;
        resurfData.maxLutExtent       = scaleLutMaxExtent;
        // Straw parameters
        resurfData.strawTaperPower     = strawTaperPower;
        resurfData.strawBendAmount     = strawBendAmount;
        resurfData.strawBaseRadius     = strawBaseRadius;
        resurfData.strawBendDirection  = strawBendDirection;
        resurfData.strawBendRandomness = strawBendRandomness;
        // Stud parameters
        resurfData.studElongation         = studElongation;
        resurfData.studHeight             = studHeight;
        resurfData.studPower              = studPower;
        resurfData.studRotation           = studRotation;
        resurfData.studRotationRandomness = studRotationRandomness;
        resurfData.studTreadPlate         = studTreadPlate ? 1u : 0u;
        // hasPreprocessData bitmask: bit 0 = curvature density, bit 1 = feature edges
        resurfData.hasPreprocessData = 0u;
        if (preprocessLoaded && enablePreprocess) {
            if (enableCurvatureDensity) resurfData.hasPreprocessData |= 1u;
            if (enableFeatureEdges)     resurfData.hasPreprocessData |= 2u;
        }
        resurfData.preprocessSlotsPerFace = slotsPerFace;
        resurfData.preprocessCurvatureScale = preprocessCurvatureScale;
        resurfData.preprocessCurvatureBoost = preprocessCurvatureBoost;
        resurfData.featureEdgeBoostUBO = featureEdgeBoost;
        resurfData.grwmIntensityUBO = grwmIntensity;
        resurfData.enableSpecularAA = enableSpecularAA ? 1u : 0u;
        resurfData.specularAAStrengthUBO = specularAAStrength;
        resurfData.enableCoverageFade = enableCoverageFade ? 1u : 0u;
        resurfData.coverageFadeStartUBO = coverageFadeStart;
        resurfData.coverageFadeEndUBO = coverageFadeEnd;
        resurfData.enableProxyUBO = enableProxy ? 1u : 0u;
        resurfData.proxyStartThresholdUBO = proxyStartThreshold;
        resurfData.proxyEndThresholdUBO = proxyEndThreshold;
        resurfData.hasDiffuseTexture = diffuseTextureLoaded ? 1u : 0u;
        resurfData.hasEnvMap = (skyboxLoaded && showSkybox) ? 1u : 0u;
        resurfData.hasNormalTexture = normalTextureLoaded ? 1u : 0u;
        resurfData.hasOrmTexture = ormTextureLoaded ? 1u : 0u;
        memcpy(resurfacingUBOMapped, &resurfData, sizeof(ResurfacingUBO));
    }

    // Update secondary ResurfacingUBO (used when dualMeshActive)
    if (dualMeshActive) {
        ResurfacingUBO secData{};
        secData.elementType      = secondaryElementType;
        secData.userScaling      = secondaryUserScaling;
        secData.resolutionM      = secondaryResolutionM;
        secData.resolutionN      = secondaryResolutionN;
        secData.torusMajorR      = secondaryTorusMajorR;
        secData.torusMinorR      = secondaryTorusMinorR;
        secData.sphereRadius     = secondarySphereRadius;
        secData.doLod            = enableLod ? 1u : 0u;
        secData.lodFactor        = lodFactor;
        secData.Nx               = scaleLutNx;
        secData.Ny               = scaleLutNy;
        secData.normalPerturbation = secondaryNormalPerturbation;
        secData.minLutExtent     = scaleLutMinExtent;
        secData.maxLutExtent     = scaleLutMaxExtent;
        // Secondary mesh shares skinning state with primary
        secData.doSkinning       = (skeletonLoaded && doSkinning) ? 1u : 0u;
        memcpy(secondaryResurfacingUBOMapped, &secData, sizeof(ResurfacingUBO));
    }

    PushConstants pushConstants{};               // Per-draw constants shared by task/mesh/frag

    glm::mat4 baseModel = thirdPersonMode ? player.getModelMatrix() : glm::mat4(1.0f); // Player transform in 3rd-person, else identity
    if (turntableMode) {                         // Turntable mode adds the UI drag rotation
        baseModel = glm::mat4_cast(objectRotation) * baseModel;
    }
    pushConstants.model = baseModel;             // Object→world matrix
    pushConstants.nbFaces = heNbFaces;           // Mesh element counts (for vertex-element indexing)
    pushConstants.nbVertices = heNbVertices;
    pushConstants.elementType = elementType;     // Which procedural element to grow
    pushConstants.userScaling = userScaling;     // Global element size
    pushConstants.torusMajorR = torusMajorR;     // Shape params (only the relevant ones used)
    pushConstants.torusMinorR = torusMinorR;
    pushConstants.sphereRadius = sphereRadius;
    pushConstants.resolutionM = resolutionM;     // Tessellation resolution
    pushConstants.resolutionN = resolutionN;
    pushConstants.debugMode = debugMode;         // Debug visualization selector
    pushConstants.enableCulling = (enableFrustumCulling ? 1u : 0u) | (enableBackfaceCulling ? 2u : 0u) // Culling bitmask:
                                | ((useMaskTexture && maskTextureLoaded) ? 4u : 0u); // bit0 frustum, bit1 backface, bit2 mask
    pushConstants.cullingThreshold = cullingThreshold; // Backface dot threshold
    pushConstants.enableLod = enableLod ? 1u : 0u;     // Adaptive LOD flag
    pushConstants.lodFactor = lodFactor;
    pushConstants.chainmailMode = chainmailMode ? 1u : 0u; // Chainmail layout flag
    pushConstants.chainmailTiltAngle = chainmailTiltAngle;
    pushConstants.chainmailSurfaceOffset = chainmailSurfaceOffset;
    pushConstants.activeSlots = (enableSlotPlacement && preprocessLoaded && enablePreprocess) // GRWM slot count (0 = off)
        ? static_cast<uint32_t>(activeSlotCount) : 0u;
    pushConstants.slotUniformSizeFlag = slotUniformSize ? 1u : 0u; // Don't shrink elements per slot count

    vkCmdPushConstants(cmd, pipelineLayout,      // Push the constants to all three stages
                        VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(PushConstants), &pushConstants);
    if (renderResurfacing && !renderPebbles) {   // Resurfacing draw path (mutually exclusive with pebbles)
        if (heMeshUploaded) {                    // Only if a mesh is loaded
            // CPU pre-cull: build compact visible element index list.
            // Only rebuilt when camera, scale, or culling settings change.
            bool doMaskCull = useMaskTexture && maskTextureLoaded && !cpuMaskPixels.empty(); // Mask culling active?
            bool doCulling   = enableFrustumCulling || enableBackfaceCulling; // Any geometric culling?

            float aspect = static_cast<float>(swapChainExtent.width) / // Aspect for the MVP
                           static_cast<float>(swapChainExtent.height);
            glm::mat4 model = thirdPersonMode ? player.getModelMatrix() : glm::mat4(1.0f); // Same model matrix as the push constant
            if (turntableMode) {
                model = glm::mat4_cast(objectRotation) * model;
            }
            glm::mat3 modelNormalMat = glm::mat3(model);  // for transforming normals
            glm::mat4 mvp = activeCamera->getProjectionMatrix(aspect) * // Object→clip, for the culling test
                            activeCamera->getViewMatrix() * model;

            uint32_t slotK = (enableSlotPlacement && preprocessLoaded && enablePreprocess) // Slots-per-face (0 = off)
                ? static_cast<uint32_t>(activeSlotCount) : 0u;

            bool settingsChanged = (enableFrustumCulling  != lastEnableFrustumCulling) // Did any cull-affecting setting change?
                                || (enableBackfaceCulling != lastEnableBackfaceCulling)
                                || (cullingThreshold      != lastCullingThreshold)
                                || (userScaling           != lastCullUserScaling)
                                || (doMaskCull            != lastDoMaskCull)
                                || (slotK                 != lastSlotK);
            bool cameraChanged = (mvp != lastCullMVP); // Did the camera/model move?

            if (visibleCacheDirty || settingsChanged || cameraChanged) { // Rebuild the visible list only when needed
                cachedVisibleIndices.clear();    // Reset the cached list
                cachedTotalElements = 0;

                auto isMasked = [&](glm::vec2 uv) -> bool { // True if this UV falls in a masked-out region
                    if (!doMaskCull) return false;
                    uint32_t x = static_cast<uint32_t>(uv.x * cpuMaskWidth)  % cpuMaskWidth;  // Wrap into mask texels
                    uint32_t y = static_cast<uint32_t>(uv.y * cpuMaskHeight) % cpuMaskHeight;
                    return cpuMaskPixels[y * cpuMaskWidth + x] < 128; // Dark = masked out
                };

                auto isVisible = [&](glm::vec3 pos, glm::vec3 normal, float area) -> bool { // Frustum + backface test (mirrors the task shader)
                    if (!doCulling) return true;
                    float radius = std::sqrt(area) * userScaling * 2.0f; // Conservative element radius
                    if (enableFrustumCulling) {
                        glm::vec4 clip = mvp * glm::vec4(pos, 1.0f); // Project to clip space
                        if (clip.w <= 0.0f) return false;            // Behind the camera
                        float cr = radius / clip.w * 2.0f * 1.1f;    // Radius in NDC (with margin)
                        glm::vec3 ndc = glm::vec3(clip) / clip.w;    // NDC center
                        if (ndc.x + cr < -1.0f || ndc.x - cr > 1.0f) return false; // Left/right
                        if (ndc.y + cr < -1.0f || ndc.y - cr > 1.0f) return false; // Bottom/top
                        if (ndc.z + cr <  0.0f || ndc.z - cr > 1.0f) return false; // Near/far (Vulkan [0,1])
                    }
                    if (enableBackfaceCulling) {
                        glm::vec3 worldPos = glm::vec3(model * glm::vec4(pos, 1.0f)); // Element world position
                        glm::vec3 worldNormal = glm::normalize(modelNormalMat * normal); // World normal
                        glm::vec3 viewDir = glm::normalize(activeCamera->getPosition() - worldPos); // Toward camera
                        if (glm::dot(viewDir, worldNormal) <= cullingThreshold) return false; // Facing away → cull
                    }
                    return true;                 // Survives all tests
                };

                uint32_t totalElements = heNbFaces + heNbVertices; // Faces + vertex elements
                cachedVisibleIndices.reserve(std::min(totalElements, VISIBLE_INDICES_MAX)); // Avoid reallocations

                auto cullStart = std::chrono::high_resolution_clock::now(); // Time the cull for the stats UI

                if (slotK > 0) {                 // Slot placement mode
                    // Slot mode: emit K indices per visible face
                    for (uint32_t i = 0; i < heNbFaces; i++) { // Faces only (no vertex elements in slot mode)
                        if (isMasked(cpuFaceUVs[i])) continue; // Skip masked faces
                        cachedTotalElements += slotK; // Count K elements per face
                        if (isVisible(cpuFaceCenters[i], cpuFaceNormals[i], cpuFaceAreas[i])) { // Visible?
                            for (uint32_t s = 0; s < slotK; s++) { // Emit one compound index per slot
                                if (cachedVisibleIndices.size() < VISIBLE_INDICES_MAX)
                                    cachedVisibleIndices.push_back(i * slotK + s); // faceId*K + slot
                            }
                        }
                    }
                    // Skip vertex elements in slot mode
                } else {                         // Regular (one element per face/vertex) mode
                    for (uint32_t i = 0; i < heNbFaces; i++) { // Face elements
                        if (isMasked(cpuFaceUVs[i])) continue;
                        cachedTotalElements++;
                        if (isVisible(cpuFaceCenters[i], cpuFaceNormals[i], cpuFaceAreas[i]))
                            if (cachedVisibleIndices.size() < VISIBLE_INDICES_MAX)
                                cachedVisibleIndices.push_back(i); // Face index
                    }
                    for (uint32_t i = 0; i < heNbVertices; i++) { // Vertex elements
                        if (isMasked(cpuVertexUVs[i])) continue;
                        cachedTotalElements++;
                        if (isVisible(cpuVertexPositions[i], cpuVertexNormals[i], cpuVertexFaceAreas[i]))
                            if (cachedVisibleIndices.size() < VISIBLE_INDICES_MAX)
                                cachedVisibleIndices.push_back(heNbFaces + i); // Vertex elements indexed after faces
                    }
                }

                cpuCullTimeMs = std::chrono::duration<float, std::milli>( // Record cull time (ms) for the UI
                    std::chrono::high_resolution_clock::now() - cullStart).count();

                lastCullMVP                 = mvp; // Remember the inputs so we can skip rebuilds next frame
                lastEnableFrustumCulling    = enableFrustumCulling;
                lastEnableBackfaceCulling   = enableBackfaceCulling;
                lastCullingThreshold        = cullingThreshold;
                lastCullUserScaling         = userScaling;
                lastDoMaskCull              = doMaskCull;
                lastSlotK                   = slotK;
                visibleCacheDirty           = false; // Cache is now clean
            }

            // Upload visible indices to GPU buffer and dispatch
            uint32_t visibleCount = static_cast<uint32_t>(cachedVisibleIndices.size()); // How many elements survived

            // Compute CPU-estimated mesh shader workgroup count (LOD off, tile grid per element)
            if (!enableLod && visibleCount > 0) { // (Stats estimate only; mirrors the task shader's tile math)
                uint32_t M = resolutionM, N = resolutionN;
                uint32_t dU = M, dV = N;          // Start with the full element as one tile
                if ((dU + 1) * (dV + 1) > 256) {  // Clamp to the 256-vertex mesh limit
                    uint32_t maxD = static_cast<uint32_t>(std::sqrt(256.0f)) - 1;
                    dU = std::min(dU, maxD);
                    dV = std::min(dV, maxD);
                }
                if (dU * dV * 2 > 256) {          // Clamp to the 256-primitive limit
                    uint32_t maxD = static_cast<uint32_t>(std::sqrt(256.0f / 2.0f));
                    dU = std::min(dU, maxD);
                    dV = std::min(dV, maxD);
                }
                dU = std::max(dU, 2u);            // Minimum tile size
                dV = std::max(dV, 2u);
                uint32_t tilesU = (M + dU - 1) / dU; // Tiles needed in U
                uint32_t tilesV = (N + dV - 1) / dV; // Tiles needed in V
                cachedEstMeshShaders = visibleCount * tilesU * tilesV; // Estimated mesh workgroups
            } else {
                cachedEstMeshShaders = 0;         // Unknown when LOD is on (task shader decides)
            }

            if (visibleCount > 0) {               // Only draw if something is visible
                memcpy(visibleIndicesMapped[currentFrame], // Upload the visible-index list for the task shader
                       cachedVisibleIndices.data(),
                       visibleCount * sizeof(uint32_t));
                pfnCmdDrawMeshTasksEXT(cmd, visibleCount, 1, 1); // One task workgroup per visible element
                frameDrawCalls++;
            }
        } else {                                 // No mesh loaded → dispatch a single dummy workgroup
            cachedVisibleIndices.clear();
            cachedTotalElements = 0;
            cachedEstMeshShaders = 0;
            cpuCullTimeMs = 0.0f;
            pfnCmdDrawMeshTasksEXT(cmd, 1, 1, 1); // Keeps the pipeline "warm" / valid
            frameDrawCalls++;
        }
    }

    // Pebble pipeline draw path
    if (renderPebbles && heMeshUploaded) {       // Pebbles instead of resurfacing (mutually exclusive)
        // Update PebbleUBO
        pebbleUBO.hasAOTexture = (useAOTexture && aoTextureLoaded) ? 1u : 0u; // AO texture present?
        pebbleUBO.doSkinning = (skeletonLoaded && doSkinning) ? 1u : 0u;      // Skinning active?
        // Sync universal culling/LOD settings into pebble UBO
        pebbleUBO.useCulling = (enableFrustumCulling || enableBackfaceCulling) ? 1u : 0u; // Share the global culling toggles
        pebbleUBO.cullingThreshold = cullingThreshold;
        pebbleUBO.useLod = enableLod ? 1u : 0u;
        pebbleUBO.lodFactor = lodFactor;
        memcpy(pebbleUBOMapped, &pebbleUBO, sizeof(PebbleUBO)); // Upload the pebble config

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pebblePipeline); // Bind pebble pipeline
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 0: scene
                                 pipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 1: half-edge
                                 pipelineLayout, 1, 1,
                                 &heDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 2: pebble per-object (PebbleUBO)
                                 pipelineLayout, 2, 1,
                                 &pebblePerObjectDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(PushConstants), &pushConstants);
        pfnCmdDrawMeshTasksEXT(cmd, heNbFaces, 1, 1); // One task workgroup per face (no pre-cull for pebbles)
        frameDrawCalls++;
    }

    // Pebble control cage overlay
    if (showControlCage && renderPebbles && heMeshUploaded) { // Debug overlay: draw the pebble control cages
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pebbleCagePipeline); // Bind the cage (line) pipeline
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 0: scene
                                 pipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 1: half-edge
                                 pipelineLayout, 1, 1,
                                 &heDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 2: pebble per-object
                                 pipelineLayout, 2, 1,
                                 &pebblePerObjectDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(PushConstants), &pushConstants);
        pfnCmdDrawMeshTasksEXT(cmd, heNbFaces, 1, 1); // One workgroup per face (rebuilds the cage as lines)
        frameDrawCalls++;
    }

    // Ground pathway pebbles
    if (renderPathway && groundMeshActive) {     // Pebbles on the ground plane, revealed along the player's path
        // Update per-frame and pathway-specific fields (pebble appearance is controlled independently via UI)
        groundPebbleUBO.usePathway       = fogOfWar ? 1u : 0u; // Fog-of-war: only show pebbles near the path
        groundPebbleUBO.playerWorldPos   = player.position;    // Path is centered on the player
        groundPebbleUBO.pad1             = 0.0f;
        groundPebbleUBO.playerForward    = playerForwardDir();  // Path direction
        groundPebbleUBO.pad2             = 0.0f;
        groundPebbleUBO.pathwayRadius    = pathwayRadius;       // Path zone size
        groundPebbleUBO.pathwayBackScale = pathwayBackScale;    // Smaller radius behind the player
        groundPebbleUBO.pathwayFalloff   = pathwayFalloff;      // Edge softness
        groundPebbleUBO.time             = pebbleUBO.time;      // Share the animation clock
        groundPebbleUBO.doSkinning       = 0;                   // Ground never skins
        groundPebbleUBO.hasAOTexture     = 0;                   // No AO on the ground

        // Apply ground pebble scale to a copy for upload
        PebbleUBO groundUpload = groundPebbleUBO; // Upload a scaled copy (keep the UI value intact)
        groundUpload.extrusionAmount *= groundPebbleScale; // Independent ground pebble size
        memcpy(groundPebbleUBOMapped, &groundUpload, sizeof(PebbleUBO));

        // Ground plane is stationary — model matrix stays at world origin
        PushConstants groundPush = pushConstants; // Start from the shared constants...
        groundPush.model        = glm::mat4(1.0f); // ...but the ground is fixed at the origin
        groundPush.nbFaces      = groundNbFaces;    // Ground face count
        groundPush.nbVertices   = 0;                // No vertex elements on the ground
        groundPush.enableCulling &= ~4u;  // no mask texture on ground plane // Clear the mask-cull bit

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pebblePipeline); // Reuse the pebble pipeline
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 0: scene
                                 pipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 1: ground half-edge
                                 pipelineLayout, 1, 1,
                                 &groundHeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 2: ground pebble UBO
                                 pipelineLayout, 2, 1,
                                 &groundPebbleDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(PushConstants), &groundPush);
        pfnCmdDrawMeshTasksEXT(cmd, groundNbFaces, 1, 1); // One workgroup per ground face
        frameDrawCalls++;
    }

    // Ground plane wireframe overlay
    if (showGroundMesh && groundMeshActive) {    // Debug overlay: draw the ground mesh wireframe
        PushConstants gp = pushConstants;        // Ground at the origin, face count from the ground mesh
        gp.model    = glm::mat4(1.0f);
        gp.nbFaces  = groundNbFaces;
        gp.nbVertices = 0;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, baseMeshPipeline); // Reuse the base-mesh (wire) pipeline
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 0: scene
                                 pipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 1: ground half-edge
                                 pipelineLayout, 1, 1,
                                 &groundHeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 2: ground pebble set (reused)
                                 pipelineLayout, 2, 1,
                                 &groundPebbleDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(PushConstants), &gp);
        pfnCmdDrawMeshTasksEXT(cmd, groundNbFaces, 1, 1); // One workgroup per ground face
        frameDrawCalls++;
    }

    // Dual-mesh: render secondary mesh with its own independent resurfacing
    if (renderResurfacing && dualMeshActive && heMeshUploaded) { // The dragon coat, with its own resurfacing params
        PushConstants secPush{};                 // Build separate push constants for the secondary mesh
        secPush.model          = pushConstants.model;   // inherit player transform
        secPush.nbFaces        = secondaryHeNbFaces;     // Secondary face count
        secPush.nbVertices     = 0;                     // face elements only
        secPush.elementType    = secondaryElementType;   // Secondary's own element type...
        secPush.userScaling    = secondaryUserScaling;   // ...scale...
        secPush.torusMajorR    = secondaryTorusMajorR;   // ...and shape params
        secPush.torusMinorR    = secondaryTorusMinorR;
        secPush.sphereRadius   = secondarySphereRadius;
        secPush.resolutionM    = secondaryResolutionM;
        secPush.resolutionN    = secondaryResolutionN;
        secPush.debugMode      = debugMode;              // Shares the global debug mode
        secPush.enableCulling  = 0;                     // no culling for secondary
        secPush.enableLod      = enableLod ? 1u : 0u;
        secPush.lodFactor      = lodFactor;
        secPush.chainmailMode  = secondaryChainmailMode ? 1u : 0u; // Independent chainmail toggle
        secPush.chainmailTiltAngle = secondaryChainmailTiltAngle;
        secPush.chainmailSurfaceOffset = secondaryChainmailSurfaceOffset;
        secPush.useDirectIndex = 1;                     // bypass visibleIndices lookup // No pre-cull list → index directly

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline); // Same resurfacing pipeline
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 0: scene
                                 pipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 1: secondary half-edge
                                 pipelineLayout, 1, 1,
                                 &secondaryHeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 2: secondary per-object (own ResurfacingUBO)
                                 pipelineLayout, 2, 1,
                                 &secondaryPerObjectDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(PushConstants), &secPush);
        pfnCmdDrawMeshTasksEXT(cmd, secondaryHeNbFaces, 1, 1); // One workgroup per secondary face (direct-indexed)
        frameDrawCalls++;
    }

    // Base mesh overlay (0=off, 1=wireframe, 2=solid, 3=both)
    if (baseMeshMode > 0 && heMeshUploaded) {    // Optionally draw the underlying base mesh over the resurfacing
        auto drawBaseMesh = [&](VkPipeline pipeline, VkDescriptorSet heSet, // Helper: bind + draw one base-mesh pass
                                 VkDescriptorSet objSet, uint32_t nbFaces,
                                 uint32_t useDirectIdx) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); // Bind the given base-mesh pipeline
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 0: scene
                                     pipelineLayout, 0, 1,
                                     &sceneDescriptorSets[currentFrame],
                                     0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 1: half-edge
                                     pipelineLayout, 1, 1,
                                     &heSet, 0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 2: per-object
                                     pipelineLayout, 2, 1,
                                     &objSet, 0, nullptr);
            pushConstants.useDirectIndex = useDirectIdx; // Direct-index for secondary meshes
            vkCmdPushConstants(cmd, pipelineLayout,
                                VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                                VK_SHADER_STAGE_FRAGMENT_BIT,
                                0, sizeof(PushConstants), &pushConstants);
            pfnCmdDrawMeshTasksEXT(cmd, nbFaces, 1, 1); // One workgroup per face
            pushConstants.useDirectIndex = 0;    // Restore the shared push constant
            frameDrawCalls++;
        };

        // Primary base mesh (coat)
        if (baseMeshMode == 6) {                 // Mode 6: solid, per-face hash coloring (debugMode 102)
            pushConstants.debugMode = 102;        // Temporarily override the debug mode...
            drawBaseMesh(baseMeshSolidPipeline, heDescriptorSet, perObjectDescriptorSet, heNbFaces, 0);
            pushConstants.debugMode = debugMode;  // ...then restore it
        } else if (baseMeshMode == 5) {          // Mode 5: solid, skin-texture preview (debugMode 101)
            pushConstants.debugMode = 101;
            drawBaseMesh(baseMeshSolidPipeline, heDescriptorSet, perObjectDescriptorSet, heNbFaces, 0);
            pushConstants.debugMode = debugMode;
        } else if (baseMeshMode == 4) {          // Mode 4: solid, mask preview (debugMode 100)
            pushConstants.debugMode = 100;
            drawBaseMesh(baseMeshSolidPipeline, heDescriptorSet, perObjectDescriptorSet, heNbFaces, 0);
            pushConstants.debugMode = debugMode;
        } else if (baseMeshMode == 2 || baseMeshMode == 3) { // Modes 2/3: plain solid base mesh
            drawBaseMesh(baseMeshSolidPipeline, heDescriptorSet, perObjectDescriptorSet, heNbFaces, 0);
        }
        if (baseMeshMode == 1 || baseMeshMode == 3) // Modes 1/3: also draw the wireframe overlay
            drawBaseMesh(baseMeshPipeline, heDescriptorSet, perObjectDescriptorSet, heNbFaces, 0);

    }

    // Dragon body base mesh — controlled by dragonBaseMeshMode (independent of coat baseMeshMode)
    if (dragonBaseMeshMode > 0 && dualMeshActive && secondaryHeNbFaces > 0 && heMeshUploaded) { // Base-mesh overlay for the secondary mesh
        // Override nbFaces for the secondary mesh (shader checks push.nbFaces)
        uint32_t savedNbFaces = pushConstants.nbFaces; // Save the primary face count...
        pushConstants.nbFaces = secondaryHeNbFaces;     // ...and swap in the secondary's

        auto drawDragonBaseMesh = [&](VkPipeline pipeline, VkDescriptorSet heSet, // Same helper, bound to the secondary sets
                                 VkDescriptorSet objSet, uint32_t nbFaces,
                                 uint32_t useDirectIdx) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 0: scene
                                     pipelineLayout, 0, 1,
                                     &sceneDescriptorSets[currentFrame],
                                     0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 1: half-edge
                                     pipelineLayout, 1, 1,
                                     &heSet, 0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, // Set 2: per-object
                                     pipelineLayout, 2, 1,
                                     &objSet, 0, nullptr);
            pushConstants.useDirectIndex = useDirectIdx; // 1 for the secondary mesh
            vkCmdPushConstants(cmd, pipelineLayout,
                                VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                                VK_SHADER_STAGE_FRAGMENT_BIT,
                                0, sizeof(PushConstants), &pushConstants);
            pfnCmdDrawMeshTasksEXT(cmd, nbFaces, 1, 1);
            pushConstants.useDirectIndex = 0;
            frameDrawCalls++;
        };
        if (dragonBaseMeshMode == 2 || dragonBaseMeshMode == 3) { // Solid pass
            drawDragonBaseMesh(baseMeshSolidPipeline, secondaryHeDescriptorSet,
                         secondaryPerObjectDescriptorSet, secondaryHeNbFaces, 1);
        }
        if (dragonBaseMeshMode == 1 || dragonBaseMeshMode == 3) { // Wireframe pass
            drawDragonBaseMesh(baseMeshPipeline, secondaryHeDescriptorSet,
                         secondaryPerObjectDescriptorSet, secondaryHeNbFaces, 1);
        }
        pushConstants.nbFaces = savedNbFaces;    // Restore the primary face count
    }

    // Benchmark mesh (traditional vertex pipeline)
    if (renderBenchmarkMesh && benchmarkMeshLoaded) { // Optional traditional vertex-pipeline mesh (for comparison)
        float aspect = static_cast<float>(swapChainExtent.width) / // Aspect for the projection
                       static_cast<float>(swapChainExtent.height);
        BenchmarkPushConstants benchPush{};      // Benchmark uses its own MVP push constants (no UBOs)
        benchPush.model = glm::mat4(1.0f);
        benchPush.view = activeCamera->getViewMatrix();
        benchPush.projection = activeCamera->getProjectionMatrix(aspect);
        benchPush.debugMode = debugMode;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, benchmarkPipeline); // Bind the vertex/fragment pipeline
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 benchmarkPipelineLayout, 0, 1,
                                 &sceneDescriptorSets[currentFrame], 0, nullptr);
        vkCmdPushConstants(cmd, benchmarkPipelineLayout, // Push MVP to the vertex + fragment stages
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(BenchmarkPushConstants), &benchPush);

        VkBuffer vertexBuffers[] = { benchmarkVertexBuffer }; // Bind the interleaved vertex buffer
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, benchmarkIndexBuffer, 0, VK_INDEX_TYPE_UINT32); // ...and index buffer
        vkCmdDrawIndexed(cmd, benchmarkIndexCount, 1, 0, 0, 0); // Classic indexed draw
        frameDrawCalls++;
    }

    // End pipeline statistics query before ImGui (exclude UI triangles, same subpass as begin)
    vkCmdEndQuery(cmd, statsQueryPool, currentFrame); // Stop counting before the UI draws

    // Draw ImGui on top
    renderImGui(cmd);                            // The UI (not counted in the triangle stats)

    vkCmdEndRenderPass(cmd);                     // End the render pass

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) { // Finish recording
        throw std::runtime_error("Failed to record command buffer!");
    }
}

// Finish a frame: record the command buffer, submit it (wait on image-available,
// signal render-finished + the in-flight fence), present, then handle a pending
// MSAA change (full render-pass/pipeline/framebuffer/ImGui rebuild) or swapchain
// recreation, and advance to the next frame slot.
void Renderer::endFrame() {
    if (!frameStarted) return;                   // beginFrame may have bailed (e.g. swapchain out-of-date)

    recordCommandBuffer(commandBuffers[currentFrame], currentImageIndex); // Record this frame's draws

    VkSubmitInfo submitInfo{};                   // Submit the recorded command buffer
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]}; // Wait until the image is acquired...
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT // ...at the color-output stage
    };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame]; // This frame's command buffer

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]}; // Signal when rendering completes
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult submitResult = vkQueueSubmit(graphicsQueue, 1, &submitInfo, // Submit; the fence fires when done
                                          inFlightFences[currentFrame]);
    if (submitResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer! VkResult: " +
                                 std::to_string(static_cast<int>(submitResult)));
    }

    VkPresentInfoKHR presentInfo{};              // Present the rendered image
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores; // Wait for rendering to finish first

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &currentImageIndex; // The acquired image index

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo); // Queue the present

    if (pendingMsaaChange) {                     // MSAA setting changed in the UI → full rebuild
        pendingMsaaChange = false;
        msaaSamples = static_cast<VkSampleCountFlagBits>(msaaSampleCount); // New sample count
        vkDeviceWaitIdle(device);                // Must be idle before tearing things down
        // Full rebuild: render pass + pipelines + framebuffers + MSAA resources + ImGui
        cleanupSwapChain();                      // Drop swapchain-derived resources
        vkDestroyRenderPass(device, renderPass, nullptr); // Render pass depends on sample count
        createSwapChain();                       // Recreate everything with the new MSAA level
        createImageViews();
        createDepthResources();
        createMsaaColorResources();
        createRenderPass();
        createFramebuffers();
        // Pipelines reference the render pass, so recreate them
        recreatePipelines();                     // Pipelines bake in the sample count too
        // Reinitialize ImGui with new MSAA sample count
        ImGui_ImplVulkan_Shutdown();             // ImGui's Vulkan backend also bakes in MSAA
        ImGui_ImplVulkan_InitInfo initInfo{};    // Re-init it against the new render pass
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = physicalDevice;
        initInfo.Device = device;
        initInfo.QueueFamily = queueFamilyIndices.graphicsFamily.value();
        initInfo.Queue = graphicsQueue;
        initInfo.DescriptorPool = descriptorPool;
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = static_cast<uint32_t>(swapChainImageViews.size());
        initInfo.RenderPass = renderPass;
        initInfo.Subpass = 0;
        initInfo.MSAASamples = msaaSamples;
        ImGui_ImplVulkan_Init(&initInfo);
        std::cout << "MSAA changed to " << msaaSamples << "x" << std::endl;
    } else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || pendingSwapChainRecreation) { // Resize/invalidation
        pendingSwapChainRecreation = false;
        recreateSwapChain();                     // Rebuild just the swapchain
    } else if (result != VK_SUCCESS) {           // Any other present error is fatal
        throw std::runtime_error("Failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT; // Advance to the next frame-in-flight slot
    frameStarted = false;                        // Frame closed
}

// *** ************ ***

// *** AI Generated ***

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
    createMsaaColorResources();
    createFramebuffers();

    std::cout << "Swap chain recreated: " << width << "x" << height << std::endl;
}

void Renderer::waitIdle() {
    vkDeviceWaitIdle(device);
}

// ============================================================================
// Proxy Shading: Precompute aggregate PBR parameters per element type
// ============================================================================

// ============================================================================
// Skybox: Load HDR environment map and create rendering pipeline
// ============================================================================

void Renderer::scanSkyboxes() {
    skyboxNames.clear();
    skyboxPaths.clear();
    std::string dir = std::string(ASSETS_DIR) + "skybox/";
    if (!std::filesystem::is_directory(dir)) return;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".hdr" || ext == ".HDR") {
            skyboxNames.push_back(entry.path().stem().string());
            skyboxPaths.push_back(entry.path().string());
        }
    }
    std::cout << "  Found " << skyboxNames.size() << " skybox HDR maps" << std::endl;
}

void Renderer::cleanupSkyboxTexture() {
    if (skyboxImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, skyboxImageView, nullptr);
        skyboxImageView = VK_NULL_HANDLE;
    }
    if (skyboxImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, skyboxImage, nullptr);
        skyboxImage = VK_NULL_HANDLE;
    }
    if (skyboxMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, skyboxMemory, nullptr);
        skyboxMemory = VK_NULL_HANDLE;
    }
}

void Renderer::loadSkybox(const std::string& hdrPath) {
    if (!std::filesystem::exists(hdrPath)) {
        std::cout << "  Skybox HDR not found: " << hdrPath << std::endl;
        return;
    }

    // Cleanup previous texture if reloading
    vkDeviceWaitIdle(device);
    cleanupSkyboxTexture();

    // Load HDR with stbi
    int width, height, channels;
    float* pixels = stbi_loadf(hdrPath.c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        std::cerr << "  Failed to load HDR: " << hdrPath << std::endl;
        return;
    }

    VkDeviceSize imageSize = width * height * 4 * sizeof(float);

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);
    void* data;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(device, stagingMemory);
    stbi_image_free(pixels);

    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &skyboxImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create skybox image!");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, skyboxImage, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device, &allocInfo, nullptr, &skyboxMemory);
    vkBindImageMemory(device, skyboxImage, skyboxMemory, 0);

    // Transition + copy
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Transition to TRANSFER_DST
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = skyboxImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, skyboxImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to SHADER_READ
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = skyboxImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &skyboxImageView);

    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(device, &samplerInfo, nullptr, &skyboxSampler);

    // One-time setup: descriptor set layout, set, UBO, sampler, pipeline
    if (skyboxDescriptorSetLayout == VK_NULL_HANDLE) {
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding texBinding{};
        texBinding.binding = 1;
        texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texBinding.descriptorCount = 1;
        texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboBinding, texBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings.data();
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &skyboxDescriptorSetLayout);

        VkDescriptorSetAllocateInfo dsAllocInfo{};
        dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsAllocInfo.descriptorPool = descriptorPool;
        dsAllocInfo.descriptorSetCount = 1;
        dsAllocInfo.pSetLayouts = &skyboxDescriptorSetLayout;
        vkAllocateDescriptorSets(device, &dsAllocInfo, &skyboxDescriptorSet);

        createBuffer(sizeof(glm::mat4) + sizeof(float) * 4,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     skyboxUBOBuffer, skyboxUBOMemory);
        vkMapMemory(device, skyboxUBOMemory, 0, sizeof(glm::mat4) + sizeof(float) * 4, 0, &skyboxUBOMapped);

        createSkyboxPipeline();
    }

    // Write/update descriptor set (texture may have changed)
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = skyboxUBOBuffer;
    bufInfo.offset = 0;
    bufInfo.range = sizeof(glm::mat4) + sizeof(float) * 4;

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgInfo.imageView = skyboxImageView;
    imgInfo.sampler = skyboxSampler;

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = skyboxDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = skyboxDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(device, 2, writes.data(), 0, nullptr);

    // Write env map to per-object descriptor sets for PBR reflection
    {
        VkDescriptorImageInfo envImgInfo{};
        envImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        envImgInfo.imageView = skyboxImageView;
        envImgInfo.sampler = skyboxSampler;

        VkDescriptorSet dstSets[] = { perObjectDescriptorSet, pebblePerObjectDescriptorSet };
        std::vector<VkWriteDescriptorSet> envWrites;
        for (auto ds : dstSets) {
            if (ds == VK_NULL_HANDLE) continue;
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = ds;
            w.dstBinding = 7;  // BINDING_ENV_MAP
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo = &envImgInfo;
            envWrites.push_back(w);
        }
        if (!envWrites.empty()) {
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(envWrites.size()),
                                   envWrites.data(), 0, nullptr);
        }
    }

    skyboxLoaded = true;
    std::cout << "  Skybox loaded: " << width << "x" << height << " HDR" << std::endl;
}

void Renderer::createSkyboxPipeline() {
    auto vertCode = readFile(std::string(SHADER_DIR) + "skybox.vert.spv");
    auto fragCode = readFile(std::string(SHADER_DIR) + "skybox.frag.spv");

    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = msaaSamples;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;  // skybox always behind everything
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynStates;

    // Pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &skyboxDescriptorSetLayout;
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &skyboxPipelineLayout);

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = skyboxPipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &skyboxPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create skybox pipeline!");
    }

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);
}

void Renderer::precomputeProxyParams() {
    constexpr int N = 32;  // sample grid resolution (N×N = 1024 samples)
    constexpr float PI_F = 3.14159265358979323846f;

    // Helper: evaluate parametric surface normal at (u,v) for a given element type
    // Returns the local-space normal (Z = face normal direction)
    auto evalNormal = [&](int type, float u, float v) -> glm::vec3 {
        float theta, phi, cosU, sinU, cosV, sinV;
        glm::vec3 dpdu, dpdv;

        switch (type) {
        case 0: { // Torus (default majorR=1, minorR=0.3)
            float majorR = 1.0f, minorR = 0.3f;
            theta = u * 2.0f * PI_F;
            phi = v * 2.0f * PI_F;
            cosU = cosf(theta); sinU = sinf(theta);
            cosV = cosf(phi); sinV = sinf(phi);
            float tubeR = majorR + minorR * cosV;
            dpdu = glm::vec3(-tubeR * sinU, tubeR * cosU, 0.0f);
            dpdv = glm::vec3(-minorR * sinV * cosU, -minorR * sinV * sinU, minorR * cosV);
            return glm::normalize(glm::cross(dpdu, dpdv));
        }
        case 1: { // Sphere
            theta = u * 2.0f * PI_F;
            phi = v * PI_F;
            return glm::normalize(glm::vec3(
                sinf(phi) * cosf(theta),
                sinf(phi) * sinf(theta),
                cosf(phi)));
        }
        case 2: { // Cone
            float radius = 0.5f, height = 1.0f;
            theta = u * 2.0f * PI_F;
            float r = radius * (1.0f - v);
            dpdu = glm::vec3(-r * sinf(theta), r * cosf(theta), 0.0f);
            dpdv = glm::vec3(-radius * cosf(theta), -radius * sinf(theta), height);
            return glm::normalize(glm::cross(dpdu, dpdv));
        }
        case 3: { // Cylinder
            theta = u * 2.0f * PI_F;
            return glm::normalize(glm::vec3(cosf(theta), sinf(theta), 0.0f));
        }
        case 4: { // Hemisphere
            theta = u * 2.0f * PI_F;
            phi = v * 0.5f * PI_F;
            return glm::normalize(glm::vec3(
                sinf(phi) * cosf(theta),
                sinf(phi) * sinf(theta),
                cosf(phi)));
        }
        default: // Dragon scale, straw, stud, pebble — approximate as hemisphere
            theta = u * 2.0f * PI_F;
            phi = v * 0.5f * PI_F;
            return glm::normalize(glm::vec3(
                sinf(phi) * cosf(theta),
                sinf(phi) * sinf(theta),
                cosf(phi)));
        }
    };

    for (int type = 0; type < 10; type++) {
        glm::vec3 normalAccum(0.0f);
        float selfShadowAccum = 0.0f;
        float varianceAccum = 0.0f;
        int sampleCount = 0;

        // Sample N×N grid across parametric UV domain
        for (int j = 0; j < N; j++) {
            for (int i = 0; i < N; i++) {
                float u = (float(i) + 0.5f) / float(N);
                float v = (float(j) + 0.5f) / float(N);

                glm::vec3 n = evalNormal(type, u, v);
                normalAccum += n;
                // Self-shadow: how much faces upward (Z = face normal direction)
                selfShadowAccum += std::max(n.z, 0.0f);
                sampleCount++;
            }
        }

        glm::vec3 meanNormal = glm::normalize(normalAccum);
        float meanTilt = acosf(std::min(std::max(meanNormal.z, -1.0f), 1.0f));

        // Normal variance: average squared deviation from mean
        for (int j = 0; j < N; j++) {
            for (int i = 0; i < N; i++) {
                float u = (float(i) + 0.5f) / float(N);
                float v = (float(j) + 0.5f) / float(N);
                glm::vec3 n = evalNormal(type, u, v);
                glm::vec3 diff = n - meanNormal;
                varianceAccum += glm::dot(diff, diff);
            }
        }
        float normalVariance = varianceAccum / float(sampleCount);

        proxyParams[type].aggregateRoughness = sqrtf(normalVariance);
        proxyParams[type].meanNormalTilt = meanTilt;
        proxyParams[type].selfShadowScale = selfShadowAccum / float(sampleCount);
        proxyParams[type].coverageFraction = 0.7f;  // approximate; depends on userScaling

        std::cout << "  Proxy params [" << type << "]: roughness="
                  << proxyParams[type].aggregateRoughness
                  << " tilt=" << proxyParams[type].meanNormalTilt
                  << " shadow=" << proxyParams[type].selfShadowScale
                  << " coverage=" << proxyParams[type].coverageFraction << std::endl;
    }
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
    pc.chainmailSurfaceOffset = chainmailSurfaceOffset;

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

// *** ************ ***
