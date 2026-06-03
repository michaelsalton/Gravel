#include "renderer/renderer.h"
#include "renderer/renderer_mesh.h"
#include "geometry/HalfEdge.h"
#include "loaders/ObjLoader.h"
#include "loaders/ImageLoader.h"
#include "loaders/GltfLoader.h"
#include <tiny_gltf.h>
#include "core/window.h"
#include "imgui.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <stdexcept>
#include <filesystem>
#include <algorithm>
#include <set>
#include <thread>

#ifndef ASSETS_DIR
#define ASSETS_DIR ""
#endif

// *** AI Generated ***

void Renderer::scanAssetMeshes() {
    assetMeshNames.clear();
    assetMeshPaths.clear();

    std::string baseMeshDir = std::string(ASSETS_DIR) + "base_mesh/";
    std::cout << "  Scanning for meshes in: " << baseMeshDir << std::endl;
    if (baseMeshDir.empty() || !std::filesystem::is_directory(baseMeshDir)) {
        std::cout << "  Warning: base_mesh directory not found!" << std::endl;
        return;
    }

    // Recursively find all .obj files in base mesh folder
    std::vector<std::pair<std::string, std::string>> entries; // (name, path)
    for (const auto& entry : std::filesystem::recursive_directory_iterator(baseMeshDir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".obj") continue;

        std::string fullPath = entry.path().string();
        std::string stem = entry.path().stem().string();

        // Skip secondary meshes (loaded via toggle, not standalone)
        if (stem == "dragon_coat") continue;

        // Display just the filename without extension
        std::string name = stem;

        entries.push_back({name, fullPath});
    }

    // Sort alphabetically by name
    std::sort(entries.begin(), entries.end());

    for (auto& [name, path] : entries) {
        assetMeshNames.push_back(std::move(name));
        assetMeshPaths.push_back(std::move(path));
    }

    // Default to cube if available
    for (int i = 0; i < static_cast<int>(assetMeshNames.size()); i++) {
        if (assetMeshNames[i] == "cube") {
            selectedMesh = i;
            break;
        }
    }
}

// *** ************ ***

// *** Michael Salton ***

// Upload a HalfEdgeMesh's SoA arrays into GPU storage buffers + a MeshInfo UBO.
// The buffer groups (vec4[5], vec2[1], int[10], float[1]) mirror the half-edge
// descriptor set layout (bindings 0-3) so they can be written directly. Reused
// for the primary, secondary, and ground meshes via the out-params.
void Renderer::uploadHEBuffers(const HalfEdgeMesh& mesh,
                                std::vector<StorageBuffer>& vec4Bufs,    // out: 5 vec4 SSBOs
                                std::vector<StorageBuffer>& vec2Bufs,    // out: 1 vec2 SSBO
                                std::vector<StorageBuffer>& intBufs,     // out: 10 int SSBOs
                                std::vector<StorageBuffer>& floatBufs,   // out: 1 float SSBO
                                VkBuffer& meshInfoBuf, VkDeviceMemory& meshInfoMem) { // out: MeshInfo UBO
    vec4Bufs.resize(5);                          // [0]pos [1]color [2]vtxNormal [3]faceNormal [4]faceCenter
    vec2Bufs.resize(1);                          // [0]texcoords
    intBufs.resize(10);                          // topology arrays (see below)
    floatBufs.resize(1);                         // [0]faceAreas

    vec4Bufs[0].create(device, physicalDevice, mesh.vertexPositions.size() * sizeof(glm::vec4), mesh.vertexPositions.data()); // vertex positions
    vec4Bufs[1].create(device, physicalDevice, mesh.vertexColors.size() * sizeof(glm::vec4), mesh.vertexColors.data());       // vertex colors
    vec4Bufs[2].create(device, physicalDevice, mesh.vertexNormals.size() * sizeof(glm::vec4), mesh.vertexNormals.data());     // vertex normals
    vec4Bufs[3].create(device, physicalDevice, mesh.faceNormals.size() * sizeof(glm::vec4), mesh.faceNormals.data());         // face normals (w = 2-coloring)
    vec4Bufs[4].create(device, physicalDevice, mesh.faceCenters.size() * sizeof(glm::vec4), mesh.faceCenters.data());         // face centers

    vec2Bufs[0].create(device, physicalDevice, mesh.vertexTexCoords.size() * sizeof(glm::vec2), mesh.vertexTexCoords.data()); // vertex texcoords

    intBufs[0].create(device, physicalDevice, mesh.vertexEdges.size() * sizeof(int), mesh.vertexEdges.data());               // outgoing edge per vertex
    intBufs[1].create(device, physicalDevice, mesh.faceEdges.size() * sizeof(int), mesh.faceEdges.data());                   // first edge per face
    intBufs[2].create(device, physicalDevice, mesh.faceVertCounts.size() * sizeof(int), mesh.faceVertCounts.data());         // side count per face
    intBufs[3].create(device, physicalDevice, mesh.faceOffsets.size() * sizeof(int), mesh.faceOffsets.data());               // index offset per face
    intBufs[4].create(device, physicalDevice, mesh.heVertex.size() * sizeof(int), mesh.heVertex.data());                     // half-edge origin vertex
    intBufs[5].create(device, physicalDevice, mesh.heFace.size() * sizeof(int), mesh.heFace.data());                         // half-edge face
    intBufs[6].create(device, physicalDevice, mesh.heNext.size() * sizeof(int), mesh.heNext.data());                         // next half-edge
    intBufs[7].create(device, physicalDevice, mesh.hePrev.size() * sizeof(int), mesh.hePrev.data());                         // prev half-edge
    intBufs[8].create(device, physicalDevice, mesh.heTwin.size() * sizeof(int), mesh.heTwin.data());                         // twin half-edge
    intBufs[9].create(device, physicalDevice, mesh.vertexFaceIndices.size() * sizeof(int), mesh.vertexFaceIndices.data());   // flattened face vertex indices

    floatBufs[0].create(device, physicalDevice, mesh.faceAreas.size() * sizeof(float), mesh.faceAreas.data());               // face areas

    MeshInfoUBO meshInfo{};                      // Small UBO with the mesh's element counts
    meshInfo.nbVertices = mesh.nbVertices;
    meshInfo.nbFaces = mesh.nbFaces;
    meshInfo.nbHalfEdges = mesh.nbHalfEdges;
    meshInfo.slotsPerFace = 0;                   // GRWM slot count (filled later if used)

    if (meshInfoBuf != VK_NULL_HANDLE) {         // Free any previous MeshInfo buffer (reuse case)
        vkDestroyBuffer(device, meshInfoBuf, nullptr);
        vkFreeMemory(device, meshInfoMem, nullptr);
        meshInfoBuf = VK_NULL_HANDLE;
        meshInfoMem = VK_NULL_HANDLE;
    }
    createBuffer(sizeof(MeshInfoUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, // Allocate host-visible UBO
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 meshInfoBuf, meshInfoMem);

    void* data;                                  // Map, copy the MeshInfo, unmap
    vkMapMemory(device, meshInfoMem, 0, sizeof(MeshInfoUBO), 0, &data);
    memcpy(data, &meshInfo, sizeof(MeshInfoUBO));
    vkUnmapMemory(device, meshInfoMem);
}

// Point a half-edge descriptor set (bindings 0-3) at the given SSBO groups.
// Mirrors the array layout declared in createDescriptorSetLayouts.
void Renderer::writeHEDescriptorSet(VkDescriptorSet dstSet,
                                      const std::vector<StorageBuffer>& vec4Bufs,
                                      const std::vector<StorageBuffer>& vec2Bufs,
                                      const std::vector<StorageBuffer>& intBufs,
                                      const std::vector<StorageBuffer>& floatBufs) {
    std::vector<VkWriteDescriptorSet> writes;    // Collected writes, submitted once at the end

    std::vector<VkDescriptorBufferInfo> vec4Infos(5); // Binding 0: array of 5 vec4 buffers
    for (int i = 0; i < 5; ++i) {
        vec4Infos[i].buffer = vec4Bufs[i].getBuffer();
        vec4Infos[i].offset = 0;
        vec4Infos[i].range = vec4Bufs[i].getSize();
    }
    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = dstSet;
    w.dstBinding = 0;                            // binding 0 = vec4[5]
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.descriptorCount = 5;
    w.pBufferInfo = vec4Infos.data();
    writes.push_back(w);

    VkDescriptorBufferInfo vec2Info{};           // Binding 1: single vec2 buffer (texcoords)
    vec2Info.buffer = vec2Bufs[0].getBuffer();
    vec2Info.offset = 0;
    vec2Info.range = vec2Bufs[0].getSize();
    w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = dstSet;
    w.dstBinding = 1;                            // binding 1 = vec2[1]
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo = &vec2Info;
    writes.push_back(w);

    std::vector<VkDescriptorBufferInfo> intInfos(10); // Binding 2: array of 10 int buffers (topology)
    for (int i = 0; i < 10; ++i) {
        intInfos[i].buffer = intBufs[i].getBuffer();
        intInfos[i].offset = 0;
        intInfos[i].range = intBufs[i].getSize();
    }
    w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = dstSet;
    w.dstBinding = 2;                            // binding 2 = int[10]
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.descriptorCount = 10;
    w.pBufferInfo = intInfos.data();
    writes.push_back(w);

    VkDescriptorBufferInfo floatInfo{};          // Binding 3: single float buffer (face areas)
    floatInfo.buffer = floatBufs[0].getBuffer();
    floatInfo.offset = 0;
    floatInfo.range = floatBufs[0].getSize();
    w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = dstSet;
    w.dstBinding = 3;                            // binding 3 = float[1]
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo = &floatInfo;
    writes.push_back(w);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), // Apply all four writes at once
                           writes.data(), 0, nullptr);
}

// Upload the PRIMARY mesh to the GPU and cache CPU-side copies of the data the
// per-frame CPU pre-cull needs (face/vertex centers, normals, areas, UVs).
void Renderer::uploadHalfEdgeMesh(const HalfEdgeMesh& mesh) {
    std::cout << "Uploading half-edge mesh to GPU..." << std::endl;

    uploadHEBuffers(mesh, heVec4Buffers, heVec2Buffers, heIntBuffers, heFloatBuffers, // GPU upload into the primary buffers
                    meshInfoBuffer, meshInfoMemory);
    writeHEDescriptorSet(heDescriptorSet, heVec4Buffers, heVec2Buffers, heIntBuffers, heFloatBuffers); // Bind them

    // Store CPU copies for per-frame stats computation
    cpuFaceCenters.resize(mesh.nbFaces);         // CPU mirrors used by the pre-cull in recordCommandBuffer
    cpuFaceNormals.resize(mesh.nbFaces);
    cpuFaceAreas = mesh.faceAreas;               // Areas/vert-counts copied wholesale
    cpuFaceVertCounts = mesh.faceVertCounts;
    cpuVertexPositions.resize(mesh.nbVertices);
    cpuVertexNormals.resize(mesh.nbVertices);
    cpuVertexFaceAreas.resize(mesh.nbVertices);
    for (uint32_t i = 0; i < mesh.nbFaces; i++) { // Demote vec4 → vec3 for face center/normal
        cpuFaceCenters[i] = glm::vec3(mesh.faceCenters[i]);
        cpuFaceNormals[i] = glm::vec3(mesh.faceNormals[i]);
    }
    cpuFaceUVs.resize(mesh.nbFaces);
    for (uint32_t i = 0; i < mesh.nbFaces; i++) {
        // Mirror GPU logic: face baseUV = texcoord of first vertex of face
        int edge = mesh.faceEdges[i];            // First half-edge of the face
        uint32_t firstVert = static_cast<uint32_t>(mesh.heVertex[edge]); // Its origin vertex
        cpuFaceUVs[i] = mesh.vertexTexCoords[firstVert]; // Use that vertex's UV as the face UV (matches the shader)
    }
    cpuVertexUVs.resize(mesh.nbVertices);
    for (uint32_t i = 0; i < mesh.nbVertices; i++) { // Cache per-vertex pos/normal/UV + adjacent face area
        cpuVertexPositions[i] = glm::vec3(mesh.vertexPositions[i]);
        cpuVertexNormals[i]   = glm::vec3(mesh.vertexNormals[i]);
        int edge = mesh.vertexEdges[i];          // Outgoing edge of the vertex
        cpuVertexFaceAreas[i] = (edge >= 0) ? mesh.faceAreas[mesh.heFace[edge]] : 0.0f; // Borrow adjacent face area (0 if isolated)
        cpuVertexUVs[i] = mesh.vertexTexCoords[i];
    }

    heMeshUploaded = true;                       // Mark primary mesh present
    visibleCacheDirty = true;                    // Force a pre-cull rebuild next frame
    heNbFaces = mesh.nbFaces;                    // Cache counts for dispatch sizing
    heNbVertices = mesh.nbVertices;
    heNbHalfEdges = mesh.nbHalfEdges;
    baseMeshTriCount = 0;                         // Base-mesh triangle count = Σ (sides - 2) per face
    for (uint32_t i = 0; i < mesh.nbFaces; i++)
        baseMeshTriCount += static_cast<uint32_t>(mesh.faceVertCounts[i]) - 2;

    size_t vram = calculateVRAM();               // Report total GPU memory in use
    std::cout << "Half-edge mesh uploaded to GPU" << std::endl;
    std::cout << "  Total VRAM: " << vram / 1024.0f << " KB" << std::endl;
}

// Bind the primary ResurfacingUBO into the per-object set (binding 0).
void Renderer::updatePerObjectDescriptorSet() {
    VkDescriptorBufferInfo uboInfo{};            // Point at the ResurfacingUBO buffer
    uboInfo.buffer = resurfacingUBOBuffer;
    uboInfo.offset = 0;
    uboInfo.range = sizeof(ResurfacingUBO);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = perObjectDescriptorSet;
    write.dstBinding = 0;  // BINDING_CONFIG_UBO   // Per-object config UBO slot
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &uboInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr); // Apply the write
}

// Write the samplers (binding 4) and the 7-slot texture array (binding 5) into
// BOTH the parametric and pebble per-object sets. Each texture slot is written
// only if that texture was actually loaded (the layout is PARTIALLY_BOUND, so
// absent slots are fine). All writes are batched and submitted once at the end.
void Renderer::writeTextureDescriptors() {
    std::vector<VkWriteDescriptorSet> writes;    // Batched writes (applied at the end)

    // Write to both parametric and pebble descriptor sets
    VkDescriptorSet dstSets[] = { perObjectDescriptorSet, pebblePerObjectDescriptorSet }; // Each block loops over both sets

    // Binding 4: Samplers [linear, nearest]
    VkDescriptorImageInfo samplerInfos[2] = {};
    samplerInfos[0].sampler = linearSampler;
    samplerInfos[1].sampler = nearestSampler;

    for (auto dstSet : dstSets) {
        VkWriteDescriptorSet samplerWrite{};
        samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        samplerWrite.dstSet = dstSet;
        samplerWrite.dstBinding = 4;  // BINDING_SAMPLERS
        samplerWrite.dstArrayElement = 0;
        samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        samplerWrite.descriptorCount = 2;
        samplerWrite.pImageInfo = samplerInfos;
        writes.push_back(samplerWrite);
    }

    // Binding 5: Textures
    VkDescriptorImageInfo textureInfos[2] = {};

    // If AO is loaded, write slot 0
    if (aoTextureLoaded) {
        textureInfos[0].imageView = aoTexture.getImageView();
        textureInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for (auto dstSet : dstSets) {
            VkWriteDescriptorSet texWrite{};
            texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            texWrite.dstSet = dstSet;
            texWrite.dstBinding = 5;  // BINDING_TEXTURES
            texWrite.dstArrayElement = 0;  // AO_TEXTURE index
            texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            texWrite.descriptorCount = 1;
            texWrite.pImageInfo = &textureInfos[0];
            writes.push_back(texWrite);
        }
    }

    // If element type map is loaded, write slot 1
    if (elementTypeTextureLoaded) {
        textureInfos[1].imageView = elementTypeTexture.getImageView();
        textureInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for (auto dstSet : dstSets) {
            VkWriteDescriptorSet texWrite{};
            texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            texWrite.dstSet = dstSet;
            texWrite.dstBinding = 5;  // BINDING_TEXTURES
            texWrite.dstArrayElement = 1;  // ELEMENT_TYPE_TEXTURE index
            texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            texWrite.descriptorCount = 1;
            texWrite.pImageInfo = &textureInfos[1];
            writes.push_back(texWrite);
        }
    }

    // If mask texture is loaded, write slot 2
    VkDescriptorImageInfo maskInfo{};
    if (maskTextureLoaded) {
        maskInfo.imageView = maskTexture.getImageView();
        maskInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for (auto dstSet : dstSets) {
            VkWriteDescriptorSet texWrite{};
            texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            texWrite.dstSet = dstSet;
            texWrite.dstBinding = 5;  // BINDING_TEXTURES
            texWrite.dstArrayElement = 2;  // MASK_TEXTURE index
            texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            texWrite.descriptorCount = 1;
            texWrite.pImageInfo = &maskInfo;
            writes.push_back(texWrite);
        }
    }

    // If skin texture is loaded, write slot 3
    VkDescriptorImageInfo skinInfo{};
    if (skinTextureLoaded) {
        skinInfo.imageView = skinTexture.getImageView();
        skinInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for (auto dstSet : dstSets) {
            VkWriteDescriptorSet texWrite{};
            texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            texWrite.dstSet = dstSet;
            texWrite.dstBinding = 5;  // BINDING_TEXTURES
            texWrite.dstArrayElement = 3;  // SKIN_TEXTURE index
            texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            texWrite.descriptorCount = 1;
            texWrite.pImageInfo = &skinInfo;
            writes.push_back(texWrite);
        }
    }

    // If diffuse texture is loaded, write slot 4
    VkDescriptorImageInfo diffuseInfo{};
    if (diffuseTextureLoaded) {
        diffuseInfo.imageView = diffuseTexture.getImageView();
        diffuseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for (auto dstSet : dstSets) {
            VkWriteDescriptorSet texWrite{};
            texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            texWrite.dstSet = dstSet;
            texWrite.dstBinding = 5;  // BINDING_TEXTURES
            texWrite.dstArrayElement = 4;  // DIFFUSE_TEXTURE index
            texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            texWrite.descriptorCount = 1;
            texWrite.pImageInfo = &diffuseInfo;
            writes.push_back(texWrite);
        }
    }

    // If normal texture is loaded, write slot 5
    VkDescriptorImageInfo normalInfo{};
    if (normalTextureLoaded) {
        normalInfo.imageView = normalTexture.getImageView();
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for (auto dstSet : dstSets) {
            VkWriteDescriptorSet texWrite{};
            texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            texWrite.dstSet = dstSet;
            texWrite.dstBinding = 5;  // BINDING_TEXTURES
            texWrite.dstArrayElement = 5;  // NORMAL_TEXTURE index
            texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            texWrite.descriptorCount = 1;
            texWrite.pImageInfo = &normalInfo;
            writes.push_back(texWrite);
        }
    }

    // If ORM texture is loaded, write slot 6
    VkDescriptorImageInfo ormInfo{};
    if (ormTextureLoaded) {
        ormInfo.imageView = ormTexture.getImageView();
        ormInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for (auto dstSet : dstSets) {
            VkWriteDescriptorSet texWrite{};
            texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            texWrite.dstSet = dstSet;
            texWrite.dstBinding = 5;  // BINDING_TEXTURES
            texWrite.dstArrayElement = 6;  // ORM_TEXTURE index
            texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            texWrite.descriptorCount = 1;
            texWrite.pImageInfo = &ormInfo;
            writes.push_back(texWrite);
        }
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

// *** ************ ***

// *** AI Generated ***

void Renderer::processInput(Window& win, float deltaTime) {
    lastDeltaTime = deltaTime;
    win.gamepad.poll();

    if (thirdPersonMode) {
        // Update orbit camera target to player chest height
        orbitCamera.setTarget(player.position + glm::vec3(0.0f, 1.5f, 0.0f));

        // Player movement uses camera's orbit yaw for direction
        player.update(win, deltaTime, orbitCamera.getYaw());

        // Drive animation from player movement state
        if (player.isMoving()) {
            animationPlaying = true;
            animationSpeed = player.getAnimationSpeed();
        } else {
            animationPlaying = false;
        }
    }

    // Turntable: left-click drag rotates the object
    if (turntableMode && !ImGui::GetIO().WantCaptureMouse) {
        auto& kb = win.keyboardMouse;
        if (kb.getMouseButton(GLFW_MOUSE_BUTTON_LEFT)) {
            float dx = kb.getMouseDeltaX();
            float dy = kb.getMouseDeltaY();
            float sensitivity = 0.005f;
            if (dx != 0.0f || dy != 0.0f) {
                // Rotate around the camera's right and up axes for intuitive drag
                glm::mat4 view = activeCamera->getViewMatrix();
                glm::vec3 camRight = glm::vec3(view[0][0], view[1][0], view[2][0]);
                glm::vec3 camUp    = glm::vec3(view[0][1], view[1][1], view[2][1]);
                float angle = glm::length(glm::vec2(dx, dy)) * sensitivity;
                glm::vec3 axis = glm::normalize(camUp * dx + camRight * dy);
                glm::quat drag = glm::angleAxis(angle, axis);
                objectRotation = glm::normalize(drag * objectRotation);
            }
        }
    }

    activeCamera->processInput(win, deltaTime);
}

size_t Renderer::calculateVRAM() const {
    size_t total = 0;
    for (const auto& buf : heVec4Buffers) total += buf.getSize();
    for (const auto& buf : heVec2Buffers) total += buf.getSize();
    for (const auto& buf : heIntBuffers) total += buf.getSize();
    for (const auto& buf : heFloatBuffers) total += buf.getSize();
    total += sizeof(MeshInfoUBO);
    if (aoTextureLoaded) total += aoTexture.getMemorySize();
    if (elementTypeTextureLoaded) total += elementTypeTexture.getMemorySize();
    if (maskTextureLoaded) total += maskTexture.getMemorySize();
    if (skinTextureLoaded) total += skinTexture.getMemorySize();
    if (skeletonLoaded) {
        total += jointIndicesBuffer.getSize();
        total += jointWeightsBuffer.getSize();
        total += boneMatricesBuffer.getSize();
    }
    if (dualMeshActive) {
        for (const auto& buf : secondaryHeVec4Buffers) total += buf.getSize();
        for (const auto& buf : secondaryHeVec2Buffers) total += buf.getSize();
        for (const auto& buf : secondaryHeIntBuffers) total += buf.getSize();
        for (const auto& buf : secondaryHeFloatBuffers) total += buf.getSize();
        total += sizeof(MeshInfoUBO);
        total += secondaryJointIndicesBuffer.getSize();
        total += secondaryJointWeightsBuffer.getSize();
    }
    // Benchmark mesh (traditional vertex pipeline)
    total += benchmarkVramBytes;
    // Ground plane mesh
    if (groundMeshActive) {
        for (const auto& buf : groundHeVec4Buffers) total += buf.getSize();
        for (const auto& buf : groundHeVec2Buffers) total += buf.getSize();
        for (const auto& buf : groundHeIntBuffers) total += buf.getSize();
        for (const auto& buf : groundHeFloatBuffers) total += buf.getSize();
        total += sizeof(MeshInfoUBO);
    }
    return total;
}

// *** ************ ***

// *** Michael Salton ***

// Destroy all mesh textures and reset every associated flag/state. Called
// before loading a new mesh so stale textures don't carry over.
void Renderer::cleanupMeshTextures() {
    aoTexture.destroy();                         // Free each GPU texture
    elementTypeTexture.destroy();
    maskTexture.destroy();
    skinTexture.destroy();
    diffuseTexture.destroy();
    normalTexture.destroy();
    ormTexture.destroy();
    aoTextureLoaded = false;                     // Mark all textures absent
    elementTypeTextureLoaded = false;
    maskTextureLoaded = false;
    skinTextureLoaded = false;
    diffuseTextureLoaded = false;
    normalTextureLoaded = false;
    ormTextureLoaded = false;
    dragonCoatAvailable = false;                 // Reset dragon-coat detection state
    dragonCoatEnabled = false;
    dragonCoatPath.clear();
    useElementTypeTexture = false;               // Reset the "use this texture" UI toggles
    useAOTexture = false;
    useMaskTexture = false;
    cpuMaskPixels.clear();                        // Drop the CPU mask copy used for pre-cull stats
    cpuMaskWidth = 0;
    cpuMaskHeight = 0;
}

// *** ************ ***

// *** AI Generated ***

void Renderer::cleanupMeshSkeleton() {
    jointIndicesBuffer.destroy();
    jointWeightsBuffer.destroy();
    boneMatricesBuffer.destroy();
    skeletonLoaded = false;
    doSkinning = false;
    animationPlaying = false;
    animationTime = 0.0f;
    boneCount = 0;
    skeleton = Skeleton{};
    animations.clear();
    jointIndicesData.clear();
    jointWeightsData.clear();
}

// *** ************ ***

// *** Michael Salton ***

// Tear down the secondary mesh (dragon coat): free its descriptor sets, HE
// buffers, MeshInfo, and skeleton buffers, and clear the dual-mesh flag.
void Renderer::cleanupSecondaryMesh() {
    // Free descriptor sets before destroying the buffers they reference
    if (secondaryHeDescriptorSet != VK_NULL_HANDLE) { // Return the HE set to the pool first...
        vkFreeDescriptorSets(device, descriptorPool, 1, &secondaryHeDescriptorSet);
        secondaryHeDescriptorSet = VK_NULL_HANDLE;
    }
    if (secondaryPerObjectDescriptorSet != VK_NULL_HANDLE) { // ...and the per-object set
        vkFreeDescriptorSets(device, descriptorPool, 1, &secondaryPerObjectDescriptorSet);
        secondaryPerObjectDescriptorSet = VK_NULL_HANDLE;
    }
    for (auto& buf : secondaryHeVec4Buffers) buf.destroy(); // Destroy each HE SSBO group
    for (auto& buf : secondaryHeVec2Buffers) buf.destroy();
    for (auto& buf : secondaryHeIntBuffers) buf.destroy();
    for (auto& buf : secondaryHeFloatBuffers) buf.destroy();
    secondaryHeVec4Buffers.clear();              // Empty the vectors
    secondaryHeVec2Buffers.clear();
    secondaryHeIntBuffers.clear();
    secondaryHeFloatBuffers.clear();
    if (secondaryMeshInfoBuffer != VK_NULL_HANDLE) { // Free the MeshInfo UBO
        vkDestroyBuffer(device, secondaryMeshInfoBuffer, nullptr);
        vkFreeMemory(device, secondaryMeshInfoMemory, nullptr);
        secondaryMeshInfoBuffer = VK_NULL_HANDLE;
        secondaryMeshInfoMemory = VK_NULL_HANDLE;
    }
    secondaryJointIndicesBuffer.destroy();       // Free the secondary skeleton buffers
    secondaryJointWeightsBuffer.destroy();
    secondaryHeNbFaces = 0;                       // Reset counts/flags
    secondaryHeNbVertices = 0;
    dualMeshActive = false;                       // No secondary mesh active anymore
}

// *** ************ ***

// *** AI Generated ***

void Renderer::cleanupBenchmarkMesh() {
    if (benchmarkVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, benchmarkVertexBuffer, nullptr);
        vkFreeMemory(device, benchmarkVertexMemory, nullptr);
        benchmarkVertexBuffer = VK_NULL_HANDLE;
        benchmarkVertexMemory = VK_NULL_HANDLE;
    }
    if (benchmarkIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, benchmarkIndexBuffer, nullptr);
        vkFreeMemory(device, benchmarkIndexMemory, nullptr);
        benchmarkIndexBuffer = VK_NULL_HANDLE;
        benchmarkIndexMemory = VK_NULL_HANDLE;
    }
    benchmarkIndexCount = 0;
    benchmarkVramBytes = 0;
    benchmarkNbFaces = 0;
    benchmarkNbVertices = 0;
    benchmarkTriCount = 0;
    benchmarkMeshLoaded = false;
}

void Renderer::loadBenchmarkMesh(const std::string& path) {
    std::cout << "Loading benchmark mesh: " << path << std::endl;

    vkDeviceWaitIdle(device);
    cleanupBenchmarkMesh();

    NGonMesh ngon = ObjLoader::load(path);
    ObjLoader::triangulate(ngon);

    benchmarkTriCount = ngon.nbFaces;
    benchmarkNbFaces = ngon.nbFaces;
    benchmarkNbVertices = ngon.nbVertices;

    // Build interleaved vertex buffer: (pos vec3, normal vec3, uv vec2) per unique vertex combo
    // and an index buffer for triangles
    struct BenchmarkVertex {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };

    // Flatten: each face is a triangle with 3 vertices
    // Use direct vertex expansion (no dedup) for simplicity and speed
    uint32_t totalVerts = ngon.nbFaces * 3;
    std::vector<BenchmarkVertex> vertices(totalVerts);
    std::vector<uint32_t> indices(totalVerts);

    for (uint32_t f = 0; f < ngon.nbFaces; f++) {
        const auto& face = ngon.faces[f];
        glm::vec3 faceNormal = glm::vec3(face.normal);

        for (int v = 0; v < 3; v++) {
            uint32_t idx = f * 3 + v;
            uint32_t vi = face.vertexIndices[v];
            const auto& pos = ngon.positions[vi];

            glm::vec3 norm = faceNormal;
            if (!face.normalIndices.empty() && face.normalIndices[v] < ngon.normals.size()) {
                norm = ngon.normals[face.normalIndices[v]];
            }

            glm::vec2 uv(0.0f);
            if (!face.texCoordIndices.empty() && face.texCoordIndices[v] < ngon.texCoords.size()) {
                uv = ngon.texCoords[face.texCoordIndices[v]];
            }

            vertices[idx] = { pos.x, pos.y, pos.z, norm.x, norm.y, norm.z, uv.x, uv.y };
            indices[idx] = idx;
        }
    }

    benchmarkIndexCount = static_cast<uint32_t>(indices.size());

    // Helper to create a GPU buffer with data
    auto createBuffer = [&](VkBufferUsageFlags usage, const void* data, size_t size,
                            VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = size;
        bufInfo.usage = usage;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufInfo, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to create benchmark buffer!");

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate benchmark buffer memory!");

        vkBindBufferMemory(device, buffer, memory, 0);

        void* mapped;
        vkMapMemory(device, memory, 0, size, 0, &mapped);
        memcpy(mapped, data, size);
        vkUnmapMemory(device, memory);
    };

    size_t vbSize = vertices.size() * sizeof(BenchmarkVertex);
    size_t ibSize = indices.size() * sizeof(uint32_t);

    createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices.data(), vbSize,
                 benchmarkVertexBuffer, benchmarkVertexMemory);
    createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data(), ibSize,
                 benchmarkIndexBuffer, benchmarkIndexMemory);

    benchmarkMeshLoaded = true;
    benchmarkVramBytes = vbSize + ibSize;
    float vramMB = static_cast<float>(benchmarkVramBytes) / (1024.0f * 1024.0f);
    std::cout << "Benchmark mesh loaded: " << benchmarkNbFaces << " triangles, "
              << benchmarkNbVertices << " unique vertices, "
              << totalVerts << " expanded vertices ("
              << vramMB << " MB VRAM)" << std::endl;
}

// *** ************ ***

// *** Michael Salton ***

// Load a second mesh (the dragon coat) that renders alongside the primary with
// its own independent resurfacing settings. Loads the OBJ → half-edge, uploads
// its buffers, allocates+writes its own HE and per-object descriptor sets,
// matches bone data from a sibling glTF (if a skeleton is loaded), and shares
// the samplers/scale-LUT with the primary. Sets dualMeshActive on success.
void Renderer::loadSecondaryMesh(const std::string& path) {
    std::cout << "  Loading secondary mesh: " << path << std::endl; // Progress log

    NGonMesh ngon = ObjLoader::load(path);       // Parse the OBJ into an n-gon mesh
    HalfEdgeMesh mesh = HalfEdgeBuilder::build(ngon); // Build half-edge connectivity
    computeFace2Coloring(mesh);                  // Assign the 0/1 checkerboard color per face

    secondaryHeNbFaces = mesh.nbFaces;           // Cache counts for dispatch sizing
    secondaryHeNbVertices = mesh.nbVertices;

    // Upload half-edge buffers
    uploadHEBuffers(mesh, secondaryHeVec4Buffers, secondaryHeVec2Buffers, // Upload SoA arrays into the secondary buffers
                    secondaryHeIntBuffers, secondaryHeFloatBuffers,
                    secondaryMeshInfoBuffer, secondaryMeshInfoMemory);

    // Allocate secondary HE descriptor set (Set 1 layout)
    VkDescriptorSetAllocateInfo heAllocInfo{};   // Allocate a half-edge set for the secondary mesh
    heAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    heAllocInfo.descriptorPool = descriptorPool; // From the shared pool
    heAllocInfo.descriptorSetCount = 1;
    heAllocInfo.pSetLayouts = &halfEdgeSetLayout; // Using the HE set layout
    if (vkAllocateDescriptorSets(device, &heAllocInfo, &secondaryHeDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate secondary HE descriptor set!");
    }

    // Write secondary HE descriptor set
    writeHEDescriptorSet(secondaryHeDescriptorSet, secondaryHeVec4Buffers, // Point the set at the secondary HE buffers
                         secondaryHeVec2Buffers, secondaryHeIntBuffers, secondaryHeFloatBuffers);

    // Allocate secondary per-object descriptor set (Set 2 layout)
    VkDescriptorSetAllocateInfo objAllocInfo{};  // Allocate a per-object set for the secondary mesh
    objAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    objAllocInfo.descriptorPool = descriptorPool;
    objAllocInfo.descriptorSetCount = 1;
    objAllocInfo.pSetLayouts = &perObjectSetLayout; // Using the per-object set layout
    if (vkAllocateDescriptorSets(device, &objAllocInfo, &secondaryPerObjectDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate secondary per-object descriptor set!");
    }

    // Write binding 0: secondary ResurfacingUBO (independent from primary)
    {
        VkDescriptorBufferInfo uboInfo{};        // Point at the secondary ResurfacingUBO (own settings)
        uboInfo.buffer = secondaryResurfacingUBOBuffer;
        uboInfo.offset = 0;
        uboInfo.range = sizeof(ResurfacingUBO);

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = secondaryPerObjectDescriptorSet;
        w.dstBinding = 0;                        // Config UBO slot
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &uboInfo;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr); // Apply
    }

    // Write binding 6: scale LUT (shared with primary — needed if secondary uses dragon scale)
    if (scaleLutBuffer.getBuffer() != VK_NULL_HANDLE) { // Only if a scale LUT is loaded
        VkDescriptorBufferInfo lutInfo{};        // Point at the shared scale-LUT buffer
        lutInfo.buffer = scaleLutBuffer.getBuffer();
        lutInfo.offset = 0;
        lutInfo.range  = scaleLutBuffer.getSize();

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = secondaryPerObjectDescriptorSet;
        w.dstBinding = 6;                        // Scale-LUT slot
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &lutInfo;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr); // Apply
    }

    // Match bone data for secondary mesh vertices (if skeleton loaded)
    if (skeletonLoaded) {                        // Only skin the coat if the primary has a skeleton
        // Re-use the already-loaded glTF model to match bone data to secondary mesh
        std::string baseName = path.substr(0, path.find_last_of('.')); // Strip extension
        std::string gltfPath = baseName + ".gltf"; // Sibling .gltf alongside the .obj
        if (std::filesystem::exists(gltfPath)) { // Only if that glTF exists
            try {
                tinygltf::Model gltfModel = GltfLoader::loadModel(gltfPath); // Load the glTF model
                std::vector<glm::vec4> secJointIndices, secJointWeights;     // Per-vertex bone bindings
                GltfLoader::matchBoneDataToObjMesh(gltfModel, ngon.positions, // Match bones to this OBJ's vertices
                                                    skeleton, secJointIndices, secJointWeights);

                if (!secJointIndices.empty()) {  // Only proceed if matching produced bindings
                    secondaryJointIndicesBuffer.create(device, physicalDevice, // Upload joint indices
                        secJointIndices.size() * sizeof(glm::vec4), secJointIndices.data());
                    secondaryJointWeightsBuffer.create(device, physicalDevice, // Upload joint weights
                        secJointWeights.size() * sizeof(glm::vec4), secJointWeights.data());

                    // Write skeleton bindings to secondary per-object descriptor set
                    std::vector<VkWriteDescriptorSet> writes; // Batched skeleton writes

                    VkDescriptorBufferInfo jointsInfo{};      // Binding 1: joint indices
                    jointsInfo.buffer = secondaryJointIndicesBuffer.getBuffer();
                    jointsInfo.offset = 0; jointsInfo.range = secondaryJointIndicesBuffer.getSize();
                    VkWriteDescriptorSet w{};
                    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    w.dstSet = secondaryPerObjectDescriptorSet;
                    w.dstBinding = 1; w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    w.descriptorCount = 1; w.pBufferInfo = &jointsInfo;
                    writes.push_back(w);

                    VkDescriptorBufferInfo weightsInfo{};     // Binding 2: joint weights
                    weightsInfo.buffer = secondaryJointWeightsBuffer.getBuffer();
                    weightsInfo.offset = 0; weightsInfo.range = secondaryJointWeightsBuffer.getSize();
                    w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    w.dstSet = secondaryPerObjectDescriptorSet;
                    w.dstBinding = 2; w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    w.descriptorCount = 1; w.pBufferInfo = &weightsInfo;
                    writes.push_back(w);

                    // Shared bone matrices buffer
                    VkDescriptorBufferInfo bonesInfo{};       // Binding 3: bone matrices (shared with primary)
                    bonesInfo.buffer = boneMatricesBuffer.getBuffer();
                    bonesInfo.offset = 0; bonesInfo.range = boneMatricesBuffer.getSize();
                    w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    w.dstSet = secondaryPerObjectDescriptorSet;
                    w.dstBinding = 3; w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    w.descriptorCount = 1; w.pBufferInfo = &bonesInfo;
                    writes.push_back(w);

                    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), // Apply all skeleton writes
                                           writes.data(), 0, nullptr);

                    std::cout << "  Secondary mesh bone data matched: " // Log skinned vertex count
                              << secJointIndices.size() << " vertices" << std::endl;
                }
            } catch (const std::exception& e) {  // glTF failure is non-fatal (coat just won't skin)
                std::cerr << "  Secondary mesh glTF error: " << e.what() << std::endl;
            }
        }
    }

    // Write samplers to secondary per-object descriptor set (shared with primary)
    if (linearSampler != VK_NULL_HANDLE) {       // Only once samplers exist
        VkDescriptorImageInfo samplerInfos[2] = {}; // Binding 4: [linear, nearest]
        samplerInfos[0].sampler = linearSampler;
        samplerInfos[1].sampler = nearestSampler;

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = secondaryPerObjectDescriptorSet;
        w.dstBinding = 4;                        // Samplers slot
        w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        w.descriptorCount = 2;
        w.pImageInfo = samplerInfos;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr); // Apply
    }

    dualMeshActive = true;                       // Enable dual-mesh rendering
    std::cout << "  Secondary mesh uploaded: " << mesh.nbFaces << " faces, " // Log result
              << mesh.nbVertices << " vertices" << std::endl;
}

// Procedurally build the ground-plane mesh that the pathway pebbles grow on.
// Supports two tilings (quad grid or non-rectangular pentagons), uploads it as
// a half-edge mesh, and allocates/reuses the ground HE + pebble descriptor sets
// and ground pebble UBO. On regeneration it destroys old GPU buffers but KEEPS
// the descriptor set handles (re-pointing them) to avoid exhausting the pool.
void Renderer::generateGroundPlane(float cellSize) {
    // Compute grid resolution from desired world size.
    uint32_t N = static_cast<uint32_t>(std::ceil(groundWorldSize / cellSize)); // Cells per side
    N = std::max(N, 4u);                         // Always at least a 4x4 grid

    // On regeneration: destroy old GPU buffers but KEEP the descriptor set handles.
    // Re-allocating descriptor sets from the pool on every regeneration would exhaust
    // the pool's sampler/SSBO slots. Instead, just update the existing sets' writes
    // to point at the new buffers.

    // --- Free old GPU buffers ---
    for (auto& buf : groundHeVec4Buffers)  buf.destroy(); // Destroy the old HE SSBO groups...
    for (auto& buf : groundHeVec2Buffers)  buf.destroy();
    for (auto& buf : groundHeIntBuffers)   buf.destroy();
    for (auto& buf : groundHeFloatBuffers) buf.destroy();
    groundHeVec4Buffers.clear();                 // ...and empty the vectors
    groundHeVec2Buffers.clear();
    groundHeIntBuffers.clear();
    groundHeFloatBuffers.clear();

    if (groundMeshInfoBuffer != VK_NULL_HANDLE) { // Free the old MeshInfo UBO
        vkDestroyBuffer(device, groundMeshInfoBuffer, nullptr);
        vkFreeMemory(device, groundMeshInfoMemory, nullptr);
        groundMeshInfoBuffer = VK_NULL_HANDLE;
        groundMeshInfoMemory = VK_NULL_HANDLE;
    }
    if (groundPebbleUBOBuffer != VK_NULL_HANDLE) { // Free the old ground pebble UBO (unmap pointer too)
        vkDestroyBuffer(device, groundPebbleUBOBuffer, nullptr);
        vkFreeMemory(device, groundPebbleUBOMemory, nullptr);
        groundPebbleUBOBuffer = VK_NULL_HANDLE;
        groundPebbleUBOMemory = VK_NULL_HANDLE;
        groundPebbleUBOMapped = nullptr;
    }

    // --- Build geometry ---
    NGonMesh ngon;                               // The polygon mesh we're about to construct

    if (groundMeshType == 0) {
        // ---- Quad grid ----
        uint32_t W = N + 1;                      // Vertices per side (N cells → N+1 verts)
        ngon.nbVertices = W * W;                 // Total grid vertices
        ngon.nbFaces    = N * N;                 // Total quad faces

        ngon.positions.resize(W * W);            // Allocate per-vertex arrays
        ngon.normals.resize(W * W, glm::vec3(0.0f, 1.0f, 0.0f)); // Flat ground → all normals +Y
        ngon.texCoords.resize(W * W, glm::vec2(0.0f));
        ngon.colors.resize(W * W, glm::vec3(1.0f)); // White

        float halfSize = (N * cellSize) * 0.5f;  // Half the plane width (to center it on the origin)
        for (uint32_t row = 0; row <= N; ++row) { // Lay out the W×W vertex grid
            for (uint32_t col = 0; col <= N; ++col) {
                uint32_t idx = row * W + col;    // Flat index of this vertex
                ngon.positions[idx] = glm::vec3(col * cellSize - halfSize, 0.0f, // Centered XZ position, Y=0
                                                 row * cellSize - halfSize);
                ngon.texCoords[idx] = glm::vec2(static_cast<float>(col) / N, // UV across the whole plane
                                                 static_cast<float>(row) / N);
            }
        }

        uint32_t faceVertexOffset = 0;           // Running offset into faceVertexIndices
        for (uint32_t row = 0; row < N; ++row) { // One quad per cell
            for (uint32_t col = 0; col < N; ++col) {
                uint32_t v0 = row * W + col;         // Bottom-left corner
                uint32_t v1 = row * W + (col + 1);   // Bottom-right
                uint32_t v2 = (row + 1) * W + (col + 1); // Top-right
                uint32_t v3 = (row + 1) * W + col;   // Top-left

                NGonFace face;
                face.vertexIndices = { v0, v1, v2, v3 }; // Quad corners (CCW)
                face.count  = 4;                     // 4 sides
                face.offset = faceVertexOffset;      // Position in the flat index array
                face.normal = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f); // +Y normal
                glm::vec3 c = (ngon.positions[v0] + ngon.positions[v1] + // Centroid = average of corners
                               ngon.positions[v2] + ngon.positions[v3]) * 0.25f;
                face.center = glm::vec4(c, 1.0f);
                face.area   = cellSize * cellSize;   // Square cell area

                ngon.faces.push_back(std::move(face)); // Store the face
                ngon.faceVertexIndices.push_back(v0);   // Append its 4 indices to the flat array
                ngon.faceVertexIndices.push_back(v1);
                ngon.faceVertexIndices.push_back(v2);
                ngon.faceVertexIndices.push_back(v3);
                faceVertexOffset += 4;               // Advance the offset
            }
        }
    } else {
        // ---- Pentagon tiling ----
        // Pairs of adjacent columns (2*pc, 2*pc+1) each become 2 pentagons sharing
        // one midpoint vertex placed at the CENTER X of the left cell (off the shared
        // edge), so the resulting faces are visually non-rectangular pentagons.
        // N is rounded up to even so all columns pair cleanly.
        if (N % 2 != 0) N += 1;                  // Force an even column count so columns pair up

        uint32_t W         = N + 1;              // Corner vertices per side
        uint32_t numPairs  = N / 2;              // Number of column-pairs
        uint32_t numMidpts = numPairs * N;  // one midpoint per row per pair // Extra midpoint verts

        ngon.nbVertices = W * W + numMidpts;     // Corner grid + midpoints
        ngon.nbFaces    = N * N;                 // Two pentagons per cell-pair-row → N*N faces total

        ngon.positions.resize(ngon.nbVertices);  // Allocate per-vertex arrays
        ngon.normals.resize(ngon.nbVertices, glm::vec3(0.0f, 1.0f, 0.0f)); // All +Y
        ngon.texCoords.resize(ngon.nbVertices, glm::vec2(0.0f));
        ngon.colors.resize(ngon.nbVertices, glm::vec3(1.0f));

        float halfSize = (N * cellSize) * 0.5f;  // Half plane width (center on origin)

        // Corner vertices
        for (uint32_t row = 0; row <= N; ++row) { // Lay out the regular W×W corner grid
            for (uint32_t col = 0; col <= N; ++col) {
                uint32_t idx = row * W + col;    // Flat index
                ngon.positions[idx] = glm::vec3(col * cellSize - halfSize, 0.0f, // Centered XZ, Y=0
                                                 row * cellSize - halfSize);
                ngon.texCoords[idx] = glm::vec2(float(col) / N, float(row) / N);
            }
        }

        // Midpoint vertices: placed at the CENTER X of the left cell (not on the shared edge)
        // so that each pentagon is visually non-rectangular.
        auto midIdx = [&](uint32_t pc, uint32_t row) -> uint32_t { // Index of a pair's midpoint vertex
            return W * W + pc * N + row;         // Midpoints stored after the corner grid
        };
        for (uint32_t pc = 0; pc < numPairs; ++pc) { // For each column-pair...
            uint32_t col_right = 2 * pc + 1;     // The pair's right column
            float x = (col_right - 0.5f) * cellSize - halfSize;  // center of left cell // Midpoint X (offset off the shared edge)
            for (uint32_t row = 0; row < N; ++row) { // One midpoint per row
                uint32_t idx = midIdx(pc, row);
                float z = (row + 0.5f) * cellSize - halfSize; // Centered between the row's corners
                ngon.positions[idx] = glm::vec3(x, 0.0f, z);
                ngon.texCoords[idx] = glm::vec2(float(col_right) / N,
                                                 float(row + 0.5f) / N);
            }
        }

        auto cornerIdx = [&](uint32_t row, uint32_t col) -> uint32_t { // Index of a corner vertex
            return row * W + col;
        };

        uint32_t faceVertexOffset = 0;           // Running offset into faceVertexIndices
        for (uint32_t pc = 0; pc < numPairs; ++pc) { // For each column-pair...
            uint32_t col_l = 2 * pc;             // Left column of the pair
            uint32_t col_r = 2 * pc + 1;         // Right column of the pair
            for (uint32_t row = 0; row < N; ++row) { // For each row in the pair
                uint32_t M = midIdx(pc, row);    // This cell's shared midpoint vertex

                // Pentagon 1 (left cell): TL → TR → midpoint → BR → BL
                {
                    uint32_t v0 = cornerIdx(row,   col_l); // Top-left
                    uint32_t v1 = cornerIdx(row,   col_r); // Top-right (shared edge)
                    uint32_t v2 = M;                       // Midpoint (the 5th, off-edge vertex)
                    uint32_t v3 = cornerIdx(row+1, col_r); // Bottom-right
                    uint32_t v4 = cornerIdx(row+1, col_l); // Bottom-left

                    glm::vec3 c = (ngon.positions[v0] + ngon.positions[v1] + // Centroid = average of 5 corners
                                   ngon.positions[v2] + ngon.positions[v3] +
                                   ngon.positions[v4]) / 5.0f;
                    NGonFace face;
                    face.vertexIndices = { v0, v1, v2, v3, v4 }; // Pentagon corners
                    face.count  = 5;                     // 5 sides
                    face.offset = faceVertexOffset;
                    face.normal = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f); // +Y
                    face.center = glm::vec4(c, 1.0f);
                    face.area   = cellSize * cellSize;   // Approximate (one cell's worth)

                    ngon.faces.push_back(std::move(face)); // Store the face
                    for (auto vi : {v0, v1, v2, v3, v4})   // Append its 5 indices
                        ngon.faceVertexIndices.push_back(vi);
                    faceVertexOffset += 5;
                }

                // Pentagon 2 (right cell): TL → TR → BR → BL → midpoint
                {
                    uint32_t v0 = cornerIdx(row,   col_r);     // Top-left (shared edge)
                    uint32_t v1 = cornerIdx(row,   col_r + 1); // Top-right
                    uint32_t v2 = cornerIdx(row+1, col_r + 1); // Bottom-right
                    uint32_t v3 = cornerIdx(row+1, col_r);     // Bottom-left
                    uint32_t v4 = M;                           // Shared midpoint (off-edge)

                    glm::vec3 c = (ngon.positions[v0] + ngon.positions[v1] + // Centroid
                                   ngon.positions[v2] + ngon.positions[v3] +
                                   ngon.positions[v4]) / 5.0f;
                    NGonFace face;
                    face.vertexIndices = { v0, v1, v2, v3, v4 }; // Pentagon corners
                    face.count  = 5;
                    face.offset = faceVertexOffset;
                    face.normal = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
                    face.center = glm::vec4(c, 1.0f);
                    face.area   = cellSize * cellSize;

                    ngon.faces.push_back(std::move(face));
                    for (auto vi : {v0, v1, v2, v3, v4})
                        ngon.faceVertexIndices.push_back(vi);
                    faceVertexOffset += 5;
                }
            }
        }
    }

    HalfEdgeMesh mesh = HalfEdgeBuilder::build(ngon); // Build half-edge connectivity from the n-gon mesh
    computeFace2Coloring(mesh);                  // Checkerboard color per face
    groundNbFaces = mesh.nbFaces;                // Cache face count for dispatch sizing

    // --- Upload new GPU buffers ---
    uploadHEBuffers(mesh, groundHeVec4Buffers, groundHeVec2Buffers, // Upload SoA arrays into the ground buffers
                    groundHeIntBuffers, groundHeFloatBuffers,
                    groundMeshInfoBuffer, groundMeshInfoMemory);

    VkDeviceSize pebbleSize = sizeof(PebbleUBO); // Allocate a fresh ground pebble UBO
    createBuffer(pebbleSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 groundPebbleUBOBuffer, groundPebbleUBOMemory);
    vkMapMemory(device, groundPebbleUBOMemory, 0, pebbleSize, 0, &groundPebbleUBOMapped); // Persistently map it
    PebbleUBO initUBO{};                         // Default-initialize...
    memcpy(groundPebbleUBOMapped, &initUBO, sizeof(PebbleUBO)); // ...and write defaults (updated per frame later)

    // --- Allocate descriptor sets only on first call; reuse on regeneration ---
    if (groundHeDescriptorSet == VK_NULL_HANDLE) { // First call only: allocate the ground HE set
        VkDescriptorSetAllocateInfo heAllocInfo{};
        heAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        heAllocInfo.descriptorPool = descriptorPool;
        heAllocInfo.descriptorSetCount = 1;
        heAllocInfo.pSetLayouts = &halfEdgeSetLayout;
        if (vkAllocateDescriptorSets(device, &heAllocInfo, &groundHeDescriptorSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate ground HE descriptor set!");
    }
    // Update HE set to point at the new buffers
    writeHEDescriptorSet(groundHeDescriptorSet, groundHeVec4Buffers, // Re-point the (possibly reused) set at new buffers
                         groundHeVec2Buffers, groundHeIntBuffers, groundHeFloatBuffers);

    if (groundPebbleDescriptorSet == VK_NULL_HANDLE) { // First call only: allocate the ground pebble per-object set
        VkDescriptorSetAllocateInfo pebbleAllocInfo{};
        pebbleAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        pebbleAllocInfo.descriptorPool = descriptorPool;
        pebbleAllocInfo.descriptorSetCount = 1;
        pebbleAllocInfo.pSetLayouts = &perObjectSetLayout;
        if (vkAllocateDescriptorSets(device, &pebbleAllocInfo, &groundPebbleDescriptorSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate ground pebble descriptor set!");

        // Write samplers once (they never change)
        if (linearSampler != VK_NULL_HANDLE) {   // Samplers are static, so write them only at allocation time
            VkDescriptorImageInfo samplerInfos[2] = {}; // Binding 4: [linear, nearest]
            samplerInfos[0].sampler = linearSampler;
            samplerInfos[1].sampler = nearestSampler;

            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = groundPebbleDescriptorSet;
            w.dstBinding = 4;                    // Samplers slot
            w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            w.descriptorCount = 2;
            w.pImageInfo = samplerInfos;
            vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
        }
    }
    // Always update UBO binding (new buffer handle each time)
    {
        VkDescriptorBufferInfo uboInfo{};        // Point binding 0 at the fresh pebble UBO
        uboInfo.buffer = groundPebbleUBOBuffer;
        uboInfo.offset = 0;
        uboInfo.range  = sizeof(PebbleUBO);

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = groundPebbleDescriptorSet;
        w.dstBinding = 0;                        // Config UBO slot
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &uboInfo;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr); // Apply (every regeneration, since the buffer changed)
    }

    groundMeshActive = true;                     // Enable ground rendering
    std::cout << "Ground plane generated: " << N << "x" << N // Log result
              << (groundMeshType == 0 ? " quads" : " pentagons")
              << " (" << groundNbFaces << " faces)" << std::endl;
}

// Fully tear down the ground mesh: destroy its HE buffers, MeshInfo UBO, and
// pebble UBO, and reset its descriptor handles / state.
void Renderer::cleanupGroundMesh() {
    for (auto& buf : groundHeVec4Buffers)  buf.destroy(); // Destroy each HE SSBO group
    for (auto& buf : groundHeVec2Buffers)  buf.destroy();
    for (auto& buf : groundHeIntBuffers)   buf.destroy();
    for (auto& buf : groundHeFloatBuffers) buf.destroy();
    groundHeVec4Buffers.clear();                 // Empty the vectors
    groundHeVec2Buffers.clear();
    groundHeIntBuffers.clear();
    groundHeFloatBuffers.clear();

    if (groundMeshInfoBuffer != VK_NULL_HANDLE) { // Free the MeshInfo UBO
        vkDestroyBuffer(device, groundMeshInfoBuffer, nullptr);
        vkFreeMemory(device, groundMeshInfoMemory, nullptr);
        groundMeshInfoBuffer = VK_NULL_HANDLE;
        groundMeshInfoMemory = VK_NULL_HANDLE;
    }

    if (groundPebbleUBOBuffer != VK_NULL_HANDLE) { // Free the ground pebble UBO (and unmap pointer)
        vkDestroyBuffer(device, groundPebbleUBOBuffer, nullptr);
        vkFreeMemory(device, groundPebbleUBOMemory, nullptr);
        groundPebbleUBOBuffer = VK_NULL_HANDLE;
        groundPebbleUBOMemory = VK_NULL_HANDLE;
        groundPebbleUBOMapped = nullptr;
    }

    groundHeDescriptorSet  = VK_NULL_HANDLE;      // Forget the descriptor sets (freed with the pool)
    groundPebbleDescriptorSet = VK_NULL_HANDLE;
    groundNbFaces    = 0;                          // Reset count/flag
    groundMeshActive = false;
}

// *** ************ ***

// *** AI Generated ***

glm::vec3 Renderer::playerForwardDir() const {
    float yawRad = glm::radians(player.yaw);
    return glm::vec3(-std::sin(yawRad), 0.0f, -std::cos(yawRad));
}

void Renderer::writeSkeletonDescriptors() {
    std::vector<VkWriteDescriptorSet> writes;

    // Write to both parametric and pebble descriptor sets
    VkDescriptorSet dstSets[] = { perObjectDescriptorSet, pebblePerObjectDescriptorSet };

    VkDescriptorBufferInfo jointsInfo{};
    jointsInfo.buffer = jointIndicesBuffer.getBuffer();
    jointsInfo.offset = 0;
    jointsInfo.range = jointIndicesBuffer.getSize();

    VkDescriptorBufferInfo weightsInfo{};
    weightsInfo.buffer = jointWeightsBuffer.getBuffer();
    weightsInfo.offset = 0;
    weightsInfo.range = jointWeightsBuffer.getSize();

    VkDescriptorBufferInfo bonesInfo{};
    bonesInfo.buffer = boneMatricesBuffer.getBuffer();
    bonesInfo.offset = 0;
    bonesInfo.range = boneMatricesBuffer.getSize();

    for (auto dstSet : dstSets) {
        VkWriteDescriptorSet jointsWrite{};
        jointsWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        jointsWrite.dstSet = dstSet;
        jointsWrite.dstBinding = 1;  // BINDING_SKIN_JOINTS
        jointsWrite.dstArrayElement = 0;
        jointsWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        jointsWrite.descriptorCount = 1;
        jointsWrite.pBufferInfo = &jointsInfo;
        writes.push_back(jointsWrite);

        VkWriteDescriptorSet weightsWrite{};
        weightsWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        weightsWrite.dstSet = dstSet;
        weightsWrite.dstBinding = 2;  // BINDING_SKIN_WEIGHTS
        weightsWrite.dstArrayElement = 0;
        weightsWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        weightsWrite.descriptorCount = 1;
        weightsWrite.pBufferInfo = &weightsInfo;
        writes.push_back(weightsWrite);

        VkWriteDescriptorSet bonesWrite{};
        bonesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bonesWrite.dstSet = dstSet;
        bonesWrite.dstBinding = 3;  // BINDING_BONE_MATRICES
        bonesWrite.dstArrayElement = 0;
        bonesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bonesWrite.descriptorCount = 1;
        bonesWrite.pBufferInfo = &bonesInfo;
        writes.push_back(bonesWrite);
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

// *** ************ ***

// *** Michael Salton ***

// Load the dragon-scale B-spline control cage from an OBJ "LUT" file. Parses
// the (position, uv) pairs, sorts them into a V-major Nx*Ny grid, computes the
// cage's bounding box, packs the positions as vec4[], uploads them as the
// scale-LUT SSBO (binding 6), and records grid dims/extents for the per-frame
// UBO. Used by the dragon-scale parametric surface (elementType 5).
void Renderer::loadScaleLut() {
    std::string path = std::string(ASSETS_DIR) + "parametric_luts/scale_lut.obj"; // LUT asset path
    std::ifstream file(path);
    if (!file.is_open()) {                       // Missing file → warn and skip (dragon scale just won't work)
        std::cerr << "loadScaleLut: cannot open " << path << std::endl;
        return;
    }

    // Parse OBJ: collect positions, UVs, and face vertex pairs (posIdx, uvIdx)
    std::vector<glm::vec3> positions;            // Raw vertex positions
    std::vector<glm::vec2> uvs;                  // Raw texcoords (define the grid ordering)
    // Use a set to deduplicate (posIdx, uvIdx) pairs from face entries
    std::vector<std::pair<int,int>> uniquePairs; // Distinct (posIdx, uvIdx) pairs, in first-seen order
    std::set<std::pair<int,int>> seenPairs;      // Dedup tracker

    std::string line;
    while (std::getline(file, line)) {           // Read the LUT OBJ line by line
        if (line.empty() || line[0] == '#') continue; // Skip blanks/comments
        std::istringstream ss(line);             // Tokenize
        std::string token;
        ss >> token;                             // Record type

        if (token == "v") {                      // Vertex position
            float x, y, z;
            ss >> x >> y >> z;
            positions.push_back({x, y, z});
        } else if (token == "vt") {              // Texture coordinate
            float u, v;
            ss >> u >> v;
            uvs.push_back({u, v});
        } else if (token == "f") {               // Face: gather its (pos,uv) vertex pairs
            // Each token: posIdx/uvIdx or posIdx/uvIdx/normalIdx
            std::string vert;
            while (ss >> vert) {                  // For each vertex reference in the face
                // Parse first index (pos), second (uv)
                int vi = 0, vti = 0;
                char* ptr = vert.data();
                vi = std::strtol(ptr, &ptr, 10); // Position index (stops at the '/')
                if (*ptr == '/') {               // If a texcoord index follows...
                    ++ptr;
                    vti = std::strtol(ptr, &ptr, 10); // ...parse it
                }
                // OBJ indices are 1-based
                std::pair<int,int> key = {vi - 1, vti - 1}; // Convert to 0-based (pos, uv) pair
                if (seenPairs.insert(key).second) // First time seeing this pair?
                    uniquePairs.push_back(key);   // Keep it (in encounter order)
            }
        }
    }
    file.close();                                // Done reading

    if (uniquePairs.empty() || uvs.empty() || positions.empty()) { // Guard against a malformed/empty LUT
        std::cerr << "loadScaleLut: empty or malformed LUT file" << std::endl;
        return;
    }

    // Sort pairs by UV: primary = V ascending, secondary = U ascending
    std::sort(uniquePairs.begin(), uniquePairs.end(), // Order into a V-major grid (row by row)
              [&](const std::pair<int,int>& a, const std::pair<int,int>& b) {
                  const glm::vec2& uvA = uvs[a.second]; // UV of pair a
                  const glm::vec2& uvB = uvs[b.second]; // UV of pair b
                  if (std::abs(uvA.y - uvB.y) > 1e-5f) return uvA.y < uvB.y; // Primary key: V (row)
                  return uvA.x < uvB.x;          // Secondary key: U (column)
              });

    // Compute grid dimensions: Nx = number of unique U values in first row
    uint32_t Nx = 0;                             // Columns = count of entries sharing the first row's V
    float firstV = uvs[uniquePairs[0].second].y; // V of the first (lowest) row
    for (auto& p : uniquePairs) {
        if (std::abs(uvs[p.second].y - firstV) < 1e-5f) Nx++; // Same row → another column
        else break;                              // Row changed → row width known
    }
    uint32_t Ny = static_cast<uint32_t>(uniquePairs.size()) / Nx; // Rows = total / columns

    // Compute bounding extents
    glm::vec3 extMin(FLT_MAX), extMax(-FLT_MAX); // AABB accumulators
    for (auto& p : uniquePairs) {                // Expand over every control point
        const glm::vec3& pos = positions[p.first];
        extMin = glm::min(extMin, pos);
        extMax = glm::max(extMax, pos);
    }

    // Pack as vec4[]
    std::vector<glm::vec4> packed;               // GPU-friendly vec4 array (w unused)
    packed.reserve(uniquePairs.size());
    for (auto& p : uniquePairs)                  // In sorted (grid) order
        packed.push_back(glm::vec4(positions[p.first], 0.0f));

    // Upload to GPU (HOST_VISIBLE — buffer is tiny, ~2 KB)
    cleanupScaleLut();                           // Free any previous LUT buffer
    scaleLutBuffer.create(device, physicalDevice, // Upload the control points
                          packed.size() * sizeof(glm::vec4),
                          packed.data());

    // Store LUT metadata as flat renderer member vars (picked up each frame by UBO upload)
    scaleLutNx        = Nx;                      // Grid width  → ResurfacingUBO.Nx
    scaleLutNy        = Ny;                      // Grid height → ResurfacingUBO.Ny
    scaleLutMinExtent = glm::vec4(extMin, 0.0f); // AABB min → normalization in the shader
    scaleLutMaxExtent = glm::vec4(extMax, 0.0f); // AABB max
    scaleLutLoaded    = true;                    // Mark loaded

    // Write descriptor (binding 6) for both per-object sets
    VkDescriptorBufferInfo lutInfo{};            // Point at the LUT buffer
    lutInfo.buffer = scaleLutBuffer.getBuffer();
    lutInfo.offset = 0;
    lutInfo.range  = VK_WHOLE_SIZE;

    VkDescriptorSet dstSets[] = { perObjectDescriptorSet, pebblePerObjectDescriptorSet }; // Bind to both pipelines
    for (auto dstSet : dstSets) {
        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = dstSet;
        write.dstBinding      = 6;  // BINDING_SCALE_LUT // Scale-LUT slot
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &lutInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr); // Apply
    }

    std::cout << "Scale LUT loaded: " << Nx << "x" << Ny // Log grid dims + point count
              << " (" << uniquePairs.size() << " control points)" << std::endl;
}

// Free the scale-LUT SSBO and clear its loaded flag.
void Renderer::cleanupScaleLut() {
    scaleLutBuffer.destroy();                    // Release the GPU buffer
    scaleLutLoaded = false;                      // Mark absent
}

// *** ************ ***

// *** AI Generated ***

void Renderer::cleanupGrwmPreprocess() {
    heCurvatureBuffer.destroy();
    heFeatureFlagsBuffer.destroy();
    heSlotsBuffer.destroy();
    preprocessLoaded = false;
    slotsPerFace = 0;
}

void Renderer::runGrwmPreprocess() {
    if (loadedMeshPath.empty()) {
        grwmStatus = "No mesh loaded";
        return;
    }

    // Auto-detect GRWM binary: try submodule path first, then fallback
    if (grwmBinaryPath.empty() || !std::filesystem::exists(grwmBinaryPath)) {
        std::string paths[] = { GRWM_BINARY_PATH, GRWM_BINARY_PATH_FALLBACK };
        grwmBinaryPath.clear();
        for (const auto& p : paths) {
            if (std::filesystem::exists(p)) {
                grwmBinaryPath = p;
                std::cout << "  GRWM binary found: " << p << std::endl;
                break;
            }
        }
    }
    if (grwmBinaryPath.empty()) {
        grwmStatus = "GRWM binary not found";
        return;
    }

    std::string dir = loadedMeshPath.substr(0, loadedMeshPath.find_last_of("/\\") + 1);
    std::string outputDir = dir + "preprocess/";
    std::filesystem::create_directories(outputDir);

    std::string cmd = grwmBinaryPath
        + " " + loadedMeshPath
        + " --output " + outputDir
        + " --slots " + std::to_string(grwmSlotsPerFace)
        + " --feature-threshold " + std::to_string(grwmFeatureThreshold)
        + " 2>&1";

    grwmStatus = "Running...";
    grwmRunning = true;

    // Run in background thread so the render loop keeps running
    std::thread([this, cmd]() {
        int result = system(cmd.c_str());

        grwmRunning = false;
        if (result == 0) {
            grwmStatus = "Done — will load next frame";
            grwmPendingLoad = true;
        } else {
            grwmStatus = "Pipeline failed (exit code " + std::to_string(result) + ")";
        }
    }).detach();
}

void Renderer::loadGrwmPreprocess(const std::string& meshPath) {
    cleanupGrwmPreprocess();

    std::string dir = meshPath.substr(0, meshPath.find_last_of("/\\") + 1);
    std::string preprocessDir = dir + "preprocess/";

    if (!std::filesystem::is_directory(preprocessDir)) {
        std::cout << "  No preprocess directory found at " << preprocessDir << std::endl;
        return;
    }

    auto readHeader = [](const std::string& path, PreprocessHeader& hdr) -> bool {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        f.read(reinterpret_cast<char*>(&hdr), sizeof(PreprocessHeader));
        return f.good() && hdr.magic == 0x47525650 && hdr.version == 1;
    };

    // Read all three headers first to validate
    PreprocessHeader curvHdr{}, featHdr{}, slotsHdr{};
    std::string curvPath = preprocessDir + "curvature.bin";
    std::string featPath = preprocessDir + "features.bin";
    std::string slotsPath = preprocessDir + "slots.bin";

    if (!readHeader(curvPath, curvHdr) || !readHeader(featPath, featHdr) || !readHeader(slotsPath, slotsHdr)) {
        std::cerr << "  Warning: GRWM preprocess files missing or invalid header" << std::endl;
        return;
    }

    // Check if we need vertex remapping (split vertices from UV/normal seams)
    bool needsVertexRemap = (curvHdr.vertex_count != heNbVertices)
                            && !cpuOriginalVertexIndices.empty()
                            && curvHdr.vertex_count == cpuOriginalVertexCount;

    if (curvHdr.vertex_count != heNbVertices && !needsVertexRemap) {
        std::cerr << "  Warning: curvature.bin vertex count (" << curvHdr.vertex_count
                  << ") != mesh (" << heNbVertices << ")" << std::endl;
        return;
    }

    // Determine if we need tri->ngon remapping.
    // GRWM always triangulates, so its face_count may differ from ours.
    bool needsRemap = (featHdr.face_count != heNbFaces);
    uint32_t grwmFaceCount = featHdr.face_count;
    uint32_t grwmSlotsPerFace = slotsHdr.slots_per_face;

    if (needsRemap) {
        // Verify the triangle count is consistent: each original face of N verts
        // produces (N-2) triangles. Sum should equal GRWM face count.
        uint32_t expectedTriCount = 0;
        for (uint32_t i = 0; i < heNbFaces; i++)
            expectedTriCount += static_cast<uint32_t>(cpuFaceVertCounts[i]) - 2;

        if (expectedTriCount != grwmFaceCount) {
            std::cerr << "  Warning: GRWM face count (" << grwmFaceCount
                      << ") != expected triangulated count (" << expectedTriCount
                      << ") from " << heNbFaces << " original faces" << std::endl;
            return;
        }
        std::cout << "  Remapping GRWM data: " << grwmFaceCount << " triangles -> "
                  << heNbFaces << " original faces" << std::endl;
    }

    // --- Curvature (per-vertex, remap if vertices were split at UV/normal seams) ---
    {
        std::vector<float> rawCurvature(curvHdr.vertex_count);
        std::ifstream f(curvPath, std::ios::binary);
        f.seekg(sizeof(PreprocessHeader));
        f.read(reinterpret_cast<char*>(rawCurvature.data()), rawCurvature.size() * sizeof(float));

        std::vector<float> curvature;
        if (needsVertexRemap) {
            curvature.resize(heNbVertices);
            for (uint32_t i = 0; i < heNbVertices; i++)
                curvature[i] = rawCurvature[cpuOriginalVertexIndices[i]];
            std::cout << "  Remapping curvature: " << curvHdr.vertex_count
                      << " original -> " << heNbVertices << " split vertices" << std::endl;
        } else {
            curvature = std::move(rawCurvature);
        }

        heCurvatureBuffer.create(device, physicalDevice,
            curvature.size() * sizeof(float), curvature.data());

        // Compute median curvature for normalization
        std::vector<float> sorted = curvature;
        std::sort(sorted.begin(), sorted.end());
        float median = sorted[sorted.size() / 2];
        preprocessCurvatureScale = (median > 1e-6f) ? (1.0f / median) : 1.0f;
        std::cout << "  Loaded curvature.bin (" << curvHdr.vertex_count
                  << " vertices, median=" << median << ")" << std::endl;
    }

    // --- Features (per-face, remap: OR child triangle flags) ---
    {
        std::vector<uint8_t> triFlags(grwmFaceCount);
        std::ifstream f(featPath, std::ios::binary);
        f.seekg(sizeof(PreprocessHeader));
        f.read(reinterpret_cast<char*>(triFlags.data()), triFlags.size());

        std::vector<uint32_t> flags(heNbFaces);
        if (needsRemap) {
            uint32_t triIdx = 0;
            for (uint32_t faceId = 0; faceId < heNbFaces; faceId++) {
                uint32_t numTris = static_cast<uint32_t>(cpuFaceVertCounts[faceId]) - 2;
                uint32_t merged = 0;
                for (uint32_t t = 0; t < numTris; t++)
                    merged |= triFlags[triIdx++];
                flags[faceId] = merged;
            }
        } else {
            for (uint32_t i = 0; i < grwmFaceCount; i++) flags[i] = triFlags[i];
        }

        heFeatureFlagsBuffer.create(device, physicalDevice,
            flags.size() * sizeof(uint32_t), flags.data());
        std::cout << "  Loaded features.bin (" << heNbFaces << " faces)" << std::endl;
    }

    // --- Slots (per-face, remap: merge child triangle slots, sort, keep top N) ---
    {
        std::vector<SlotEntry> triSlots(grwmFaceCount * grwmSlotsPerFace);
        std::ifstream f(slotsPath, std::ios::binary);
        f.seekg(sizeof(PreprocessHeader));
        f.read(reinterpret_cast<char*>(triSlots.data()), triSlots.size() * sizeof(SlotEntry));

        slotsPerFace = grwmSlotsPerFace;
        std::vector<SlotEntry> finalSlots(heNbFaces * slotsPerFace);

        if (needsRemap) {
            uint32_t triIdx = 0;
            for (uint32_t faceId = 0; faceId < heNbFaces; faceId++) {
                uint32_t numTris = static_cast<uint32_t>(cpuFaceVertCounts[faceId]) - 2;

                // Gather all slots from child triangles
                std::vector<SlotEntry> merged;
                merged.reserve(numTris * grwmSlotsPerFace);
                for (uint32_t t = 0; t < numTris; t++) {
                    uint32_t base = (triIdx + t) * grwmSlotsPerFace;
                    for (uint32_t s = 0; s < grwmSlotsPerFace; s++)
                        merged.push_back(triSlots[base + s]);
                }
                triIdx += numTris;

                // Sort by priority descending, keep top slotsPerFace
                std::sort(merged.begin(), merged.end(),
                    [](const SlotEntry& a, const SlotEntry& b) {
                        return a.priority > b.priority;
                    });

                uint32_t outBase = faceId * slotsPerFace;
                for (uint32_t s = 0; s < slotsPerFace; s++) {
                    if (s < merged.size())
                        finalSlots[outBase + s] = merged[s];
                    else
                        finalSlots[outBase + s] = {0.5f, 0.5f, 0.0f, s};
                }
            }
        } else {
            finalSlots = std::move(triSlots);
        }

        heSlotsBuffer.create(device, physicalDevice,
            finalSlots.size() * sizeof(SlotEntry), finalSlots.data());
        std::cout << "  Loaded slots.bin (" << heNbFaces << " faces x "
                  << slotsPerFace << " slots)" << std::endl;
    }

    preprocessLoaded = true;

    // Re-upload MeshInfoUBO with slotsPerFace
    MeshInfoUBO meshInfo{};
    meshInfo.nbVertices = heNbVertices;
    meshInfo.nbFaces = heNbFaces;
    meshInfo.nbHalfEdges = heNbHalfEdges;
    meshInfo.slotsPerFace = slotsPerFace;
    void* data;
    vkMapMemory(device, meshInfoMemory, 0, sizeof(MeshInfoUBO), 0, &data);
    memcpy(data, &meshInfo, sizeof(MeshInfoUBO));
    vkUnmapMemory(device, meshInfoMemory);

    std::cout << "  GRWM preprocessed data loaded successfully" << std::endl;
}

void Renderer::writeGrwmDescriptors(VkDescriptorSet dstSet) {
    if (!preprocessLoaded) return;

    VkDescriptorBufferInfo curvInfo{};
    curvInfo.buffer = heCurvatureBuffer.getBuffer();
    curvInfo.offset = 0;
    curvInfo.range  = heCurvatureBuffer.getSize();

    VkDescriptorBufferInfo featInfo{};
    featInfo.buffer = heFeatureFlagsBuffer.getBuffer();
    featInfo.offset = 0;
    featInfo.range  = heFeatureFlagsBuffer.getSize();

    VkDescriptorBufferInfo slotsInfo{};
    slotsInfo.buffer = heSlotsBuffer.getBuffer();
    slotsInfo.offset = 0;
    slotsInfo.range  = heSlotsBuffer.getSize();

    std::array<VkWriteDescriptorSet, 3> writes{};
    for (auto& w : writes) w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    writes[0].dstSet = dstSet;
    writes[0].dstBinding = 4;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &curvInfo;

    writes[1].dstSet = dstSet;
    writes[1].dstBinding = 5;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &featInfo;

    writes[2].dstSet = dstSet;
    writes[2].dstBinding = 6;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].descriptorCount = 1;
    writes[2].pBufferInfo = &slotsInfo;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

// *** ************ ***

// *** Michael Salton ***

// Load an image from disk and upload it into a VulkanTexture, setting loadedFlag
// on success. Silently no-ops if the file is missing or fails to decode, so
// callers can probe optional textures without guarding each call.
void Renderer::loadAndUploadTexture(const std::string& path, VulkanTexture& texture,
                                     VkFormat format, bool& loadedFlag) {
    if (!std::filesystem::exists(path)) return;  // Optional texture absent → leave loadedFlag false

    ImageData img = ImageLoader::load(path);     // Decode to RGBA8 on the CPU
    if (img.pixels.empty()) return;              // Decode failed → skip

    texture.create(device, physicalDevice, img.width, img.height, format); // Allocate the GPU image
    texture.uploadData(commandPool, graphicsQueue, physicalDevice,         // Stage + copy pixels to it
                       img.pixels.data(), img.pixels.size());
    loadedFlag = true;                           // Mark this texture present

    std::cout << "  Loaded texture: " << path
              << " (" << img.width << "x" << img.height << ")" << std::endl;
}

void Renderer::loadMesh(const std::string& path) {
    vkDeviceWaitIdle(device);

    // The main mesh-load entry point. Tears down the previous mesh, loads the OBJ
    // (optionally triangulating/subdividing), builds + uploads the half-edge mesh,
    // loads GRWM preprocess data, creates the proxy-face buffer, auto-detects and
    // uploads sibling textures (AO/element-type/mask/skin/diffuse/normal/ORM), and
    // auto-loads a sibling glTF skeleton + a dragon-coat secondary mesh if present.

    // Cleanup previous mesh resources
    cleanupSecondaryMesh();                      // Drop the old dragon coat...
    cleanupMeshTextures();                       // ...textures...
    cleanupMeshSkeleton();                       // ...skeleton...
    cleanupGrwmPreprocess();                     // ...and GRWM data

    loadedMeshPath = path;                       // Remember the loaded path (used by GRWM/export)
    NGonMesh ngon = ObjLoader::load(path);       // Parse the OBJ into an n-gon mesh
    if (triangulateMesh) {                       // Optional: split all faces into triangles
        ObjLoader::triangulate(ngon);
    }
    if (subdivideLevel > 0) {                     // Optional: Catmull-Clark subdivision
        ObjLoader::subdivide(ngon, subdivideLevel);
    }
    if (subdivideFlatLevel > 0) {                 // Optional: flat (linear) subdivision
        ObjLoader::subdivideFlat(ngon, subdivideFlatLevel);
    }

    // Save vertex split mapping before building half-edge (for GRWM curvature remapping)
    cpuOriginalVertexIndices = ngon.originalVertexIndices; // Maps split verts → original OBJ positions
    cpuOriginalVertexCount = ngon.originalVertexCount;     // Pre-split vertex count

    HalfEdgeMesh heMesh = HalfEdgeBuilder::build(ngon); // Build half-edge connectivity
    computeFace2Coloring(heMesh);                // Checkerboard color per face
    uploadHalfEdgeMesh(heMesh);                  // Upload to GPU + cache CPU pre-cull data

    // Load GRWM preprocessed data if available
    loadGrwmPreprocess(path);                    // Curvature/feature/slot data (optional)
    writeGrwmDescriptors(heDescriptorSet);       // Bind it (bindings 4-6)

    // Create proxy face data buffer (per-face flags written by task shader, cleared via vkCmdFillBuffer)
    {
        heProxyBuffer.destroy();                 // Drop any previous proxy buffer wrapper
        size_t proxySize = heNbFaces * 4 * sizeof(float);  // ProxyFaceData = 16 bytes // One ProxyFaceData per face

        // Need TRANSFER_DST for vkCmdFillBuffer + STORAGE_BUFFER for shader access
        VkBuffer buf; VkDeviceMemory mem;        // Raw handles (StorageBuffer can't express these usage flags)
        createBuffer(proxySize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // shader RW + fill-clearable
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // Device-local (GPU-only, cleared each frame)
            buf, mem);
        // Wrap in StorageBuffer for cleanup tracking (hacky but works)
        // Actually, just use raw buffer since StorageBuffer doesn't support custom usage flags
        if (proxyFlagBuffer != VK_NULL_HANDLE) { // Free the previous proxy buffer if any
            vkDestroyBuffer(device, proxyFlagBuffer, nullptr);
            vkFreeMemory(device, proxyFlagMemory, nullptr);
        }
        proxyFlagBuffer = buf;                   // Take ownership of the new buffer
        proxyFlagMemory = mem;
        proxyFlagSize = proxySize;               // Remember its size (for vkCmdFillBuffer)
        // Write proxy buffer descriptor
        VkDescriptorBufferInfo proxyInfo{};      // Bind it to HE set binding 7
        proxyInfo.buffer = proxyFlagBuffer;
        proxyInfo.offset = 0;
        proxyInfo.range  = proxySize;
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = heDescriptorSet;
        w.dstBinding = 7;                        // Proxy buffer slot
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &proxyInfo;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr); // Apply
    }

    // Auto-detect textures in the same directory as the mesh
    std::string dir = path.substr(0, path.find_last_of("/\\") + 1); // Directory containing the mesh

    // Determine AO texture name based on mesh filename
    std::string filename = path.substr(path.find_last_of("/\\") + 1); // Mesh filename (drives AO naming)
    if (filename.find("dragon_coat") != std::string::npos) { // Dragon coat → its own AO file
        loadAndUploadTexture(dir + "dragon_coat_ao.png", aoTexture,
                             VK_FORMAT_R8G8B8A8_SRGB, aoTextureLoaded);
    } else if (filename.find("dragon") != std::string::npos) { // Dragon body
        // Note: the AO file is named "dargon_ao.png" (typo in asset)
        loadAndUploadTexture(dir + "dargon_ao.png", aoTexture, // (asset filename has a typo)
                             VK_FORMAT_R8G8B8A8_SRGB, aoTextureLoaded);
    }

    // Element type map (shared across dragon meshes)
    loadAndUploadTexture(dir + "dragon_element_type_map_2k.png", elementTypeTexture, // Per-texel element type (no-op if absent)
                         VK_FORMAT_R8G8B8A8_UNORM, elementTypeTextureLoaded);

    // Mask texture (per-face generation mask)
    loadAndUploadTexture(dir + "mask.png", maskTexture, // Generation mask (no-op if absent)
                         VK_FORMAT_R8G8B8A8_UNORM, maskTextureLoaded);
    if (maskTextureLoaded) {                     // If a mask was loaded...
        useMaskTexture = true;  // auto-enable   // ...turn mask culling on by default
        // Keep CPU copy of mask R channel for stats
        ImageData maskImg = ImageLoader::load(dir + "mask.png"); // Re-load on CPU for the pre-cull
        cpuMaskWidth = maskImg.width;
        cpuMaskHeight = maskImg.height;
        cpuMaskPixels.resize(maskImg.width * maskImg.height);
        for (uint32_t i = 0; i < maskImg.width * maskImg.height; i++)
            cpuMaskPixels[i] = maskImg.pixels[i * 4];  // R channel only // Store just the red channel
    }

    // Skin diffuse texture
    loadAndUploadTexture(dir + "skin.png", skinTexture, // Skin map for the skin-preview debug mode
                         VK_FORMAT_R8G8B8A8_SRGB, skinTextureLoaded);

    // Diffuse texture (auto-detect common names)
    {
        std::string stem = std::filesystem::path(path).stem().string(); // Mesh name (for "<stem>_diffuse" variants)
        std::vector<std::string> candidates = {  // Common diffuse filenames to probe, in priority order
            dir + "diffuse.png", dir + "color.png", dir + "albedo.png",
            dir + stem + "_diffuse.png", dir + stem + "_color.png", dir + stem + "_albedo.png",
            dir + "diffuse.jpg", dir + "color.jpg", dir + "albedo.jpg",
            dir + stem + "_diffuse.jpg", dir + stem + "_color.jpg", dir + stem + "_albedo.jpg",
        };
        for (const auto& candidate : candidates) { // Try each candidate...
            if (std::filesystem::exists(candidate)) { // ...that actually exists
                loadAndUploadTexture(candidate, diffuseTexture, // Load it (sRGB = color data)
                                     VK_FORMAT_R8G8B8A8_SRGB, diffuseTextureLoaded);
                if (diffuseTextureLoaded) {       // First success wins
                    std::cout << "  Loaded diffuse texture: " << candidate << std::endl;
                    break;
                }
            }
        }
    }

    // Normal map (auto-detect common names)
    {
        std::string stem = std::filesystem::path(path).stem().string(); // Mesh name
        std::vector<std::string> candidates = {  // Common normal-map filenames
            dir + "normal.png", dir + "normals.png",
            dir + stem + "_normal.png", dir + stem + "_normals.png",
            dir + "normal.jpg", dir + "normals.jpg",
            dir + stem + "_normal.jpg", dir + stem + "_normals.jpg",
        };
        for (const auto& candidate : candidates) { // Try each existing candidate
            if (std::filesystem::exists(candidate)) {
                loadAndUploadTexture(candidate, normalTexture, // UNORM = linear (not color)
                                     VK_FORMAT_R8G8B8A8_UNORM, normalTextureLoaded);
                if (normalTextureLoaded) {        // First success wins
                    std::cout << "  Loaded normal texture: " << candidate << std::endl;
                    break;
                }
            }
        }
    }

    // ORM texture (Occlusion/Roughness/Metallic packed in R/G/B)
    {
        std::string stem = std::filesystem::path(path).stem().string(); // Mesh name
        std::vector<std::string> candidates = {  // Common ORM filenames
            dir + "orm.png", dir + "ORM.png",
            dir + stem + "_orm.png", dir + stem + "_ORM.png",
            dir + "orm.jpg", dir + "ORM.jpg",
            dir + stem + "_orm.jpg", dir + stem + "_ORM.jpg",
        };
        for (const auto& candidate : candidates) { // Try each existing candidate
            if (std::filesystem::exists(candidate)) {
                loadAndUploadTexture(candidate, ormTexture, // UNORM = linear packed data
                                     VK_FORMAT_R8G8B8A8_UNORM, ormTextureLoaded);
                if (ormTextureLoaded) {           // First success wins
                    std::cout << "  Loaded ORM texture: " << candidate << std::endl;
                    break;
                }
            }
        }
    }

    // Write sampler and texture descriptors if any textures were loaded
    if (aoTextureLoaded || elementTypeTextureLoaded || maskTextureLoaded || skinTextureLoaded // If any texture loaded...
        || diffuseTextureLoaded || normalTextureLoaded || ormTextureLoaded) {
        writeTextureDescriptors();               // ...bind samplers + textures into the per-object sets
    }

    // Auto-detect glTF skeleton (same name as OBJ, or any .gltf in same directory)
    std::string baseName = path.substr(0, path.find_last_of('.')); // Mesh path without extension
    std::string gltfPath = baseName + ".gltf";   // Preferred: sibling glTF with the same name
    if (!std::filesystem::exists(gltfPath)) {     // If that doesn't exist...
        // Fallback: search directory for any .gltf file
        std::string gltfDir = path.substr(0, path.find_last_of("/\\") + 1); // Mesh directory
        if (!gltfDir.empty()) {
            for (const auto& entry : std::filesystem::directory_iterator(gltfDir)) { // Scan it
                if (entry.path().extension() == ".gltf") { // First .gltf found wins
                    gltfPath = entry.path().string();
                    break;
                }
            }
        }
    }
    std::cout << "  glTF path check: " << gltfPath  // Log what we resolved
              << " exists=" << std::filesystem::exists(gltfPath) << std::endl;
    if (std::filesystem::exists(gltfPath) && subdivideLevel > 0) { // Subdivided meshes can't map to the skeleton...
        std::cout << "  Skipping glTF skeleton (subdivided mesh)" << std::endl; // ...so skip it
    }
    if (std::filesystem::exists(gltfPath) && subdivideLevel == 0) { // Load skeleton only for un-subdivided meshes
        std::cout << "  Loading glTF skeleton: " << gltfPath << std::endl;
        try {
            tinygltf::Model gltfModel = GltfLoader::loadModel(gltfPath); // Parse the glTF
            std::cout << "  glTF model loaded OK" << std::endl;
            GltfLoader::extractSkeleton(gltfModel, skeleton); // Pull out the bone hierarchy
            std::cout << "  Skeleton extracted: " << skeleton.bones.size() << " bones" << std::endl;
            GltfLoader::extractAnimations(gltfModel, skeleton, animations); // Pull out animation clips
            std::cout << "  Animations extracted: " << animations.size() << std::endl;
            GltfLoader::matchBoneDataToObjMesh(gltfModel, ngon.positions, // Match per-vertex bone bindings to OBJ verts
                                                skeleton, jointIndicesData, jointWeightsData);
            std::cout << "  Bone matching done" << std::endl;

            // Extract UVs from glTF and re-upload to GPU (OBJ has no UVs)
            std::vector<glm::vec2> gltfUVs;       // glTF often carries UVs the OBJ lacks
            GltfLoader::matchUVsToObjMesh(gltfModel, ngon.positions, skeleton, gltfUVs); // Match them to OBJ verts
            bool hasUVs = false;                  // Detect whether the glTF actually had non-zero UVs
            for (const auto& uv : gltfUVs) {
                if (uv.x != 0.0f || uv.y != 0.0f) { hasUVs = true; break; }
            }
            if (hasUVs) {                         // If so, replace the (empty) OBJ UVs on the GPU
                heVec2Buffers[0].destroy();       // Drop the old texcoord buffer
                heVec2Buffers[0].create(device, physicalDevice, // Upload the glTF UVs
                    gltfUVs.size() * sizeof(glm::vec2), gltfUVs.data());
                writeHEDescriptorSet(heDescriptorSet, heVec4Buffers, heVec2Buffers, heIntBuffers, heFloatBuffers); // Rebind
                std::cout << "  UV buffer re-uploaded from glTF data" << std::endl;

                // Sync CPU UV arrays to match GPU (glTF UVs) so CPU pre-cull uses the same data
                for (uint32_t j = 0; j < static_cast<uint32_t>(gltfUVs.size()) && j < static_cast<uint32_t>(cpuVertexUVs.size()); j++)
                    cpuVertexUVs[j] = gltfUVs[j]; // Update cached per-vertex UVs
                for (uint32_t j = 0; j < heNbFaces; j++) { // Update cached per-face UVs (first vertex of each face)
                    int edge = heMesh.faceEdges[j];
                    uint32_t firstVert = static_cast<uint32_t>(heMesh.heVertex[edge]);
                    if (firstVert < gltfUVs.size())
                        cpuFaceUVs[j] = gltfUVs[firstVert];
                }
            }

            boneCount = static_cast<uint32_t>(skeleton.bones.size()); // Cache bone count
            std::cout << "  boneCount=" << boneCount
                      << " jointIndicesData.size()=" << jointIndicesData.size() << std::endl;
            if (boneCount > 0 && !jointIndicesData.empty()) { // Only upload skinning data if it exists
                // Upload joint indices (device-local, static)
                jointIndicesBuffer.create(device, physicalDevice, // Per-vertex bone indices
                    jointIndicesData.size() * sizeof(glm::vec4),
                    jointIndicesData.data());

                // Upload joint weights (device-local, static)
                jointWeightsBuffer.create(device, physicalDevice, // Per-vertex bone weights
                    jointWeightsData.size() * sizeof(glm::vec4),
                    jointWeightsData.data());

                // Bone matrices buffer (already host-visible via StorageBuffer::create)
                std::vector<glm::mat4> boneMatrices; // Initial pose matrices (updated each frame)
                GltfLoader::computeBoneMatrices(skeleton, boneMatrices);
                boneMatricesBuffer.create(device, physicalDevice,
                    boneMatrices.size() * sizeof(glm::mat4),
                    boneMatrices.data());

                skeletonLoaded = true;            // Enable skinning
                writeSkeletonDescriptors();       // Bind joints/weights/matrices (bindings 1-3)

                std::cout << "  Skeleton uploaded: " << boneCount << " bones, "
                          << jointIndicesData.size() << " skinned vertices" << std::endl;
            }
        } catch (const std::exception& e) {       // glTF errors are non-fatal (mesh still renders unskinned)
            std::cerr << "  glTF loading error: " << e.what() << std::endl;
        } catch (...) {                            // Catch-all for non-std exceptions
            std::cerr << "  glTF loading: unknown exception" << std::endl;
        }
    } else {
        std::cout << "  No glTF file found for skeleton" << std::endl; // No skeleton → static mesh
    }

    // Check if a coat mesh exists alongside (e.g. dragon_coat.obj next to dragon.obj)
    {
        std::string coatPath = dir + "dragon_coat.obj"; // Look for a sibling coat mesh
        if (std::filesystem::exists(coatPath)) {  // If present, expose it in the UI (loaded on demand)
            dragonCoatAvailable = true;
            dragonCoatPath = coatPath;
        }
    }
}

// *** ************ ***
