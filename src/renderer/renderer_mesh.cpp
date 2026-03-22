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

    // Default to "shapes/cube" if available
    for (int i = 0; i < static_cast<int>(assetMeshNames.size()); i++) {
        if (assetMeshNames[i] == "cube") {
            selectedMesh = i;
            break;
        }
    }
}

void Renderer::uploadHEBuffers(const HalfEdgeMesh& mesh,
                                std::vector<StorageBuffer>& vec4Bufs,
                                std::vector<StorageBuffer>& vec2Bufs,
                                std::vector<StorageBuffer>& intBufs,
                                std::vector<StorageBuffer>& floatBufs,
                                VkBuffer& meshInfoBuf, VkDeviceMemory& meshInfoMem) {
    vec4Bufs.resize(5);
    vec2Bufs.resize(1);
    intBufs.resize(10);
    floatBufs.resize(1);

    vec4Bufs[0].create(device, physicalDevice, mesh.vertexPositions.size() * sizeof(glm::vec4), mesh.vertexPositions.data());
    vec4Bufs[1].create(device, physicalDevice, mesh.vertexColors.size() * sizeof(glm::vec4), mesh.vertexColors.data());
    vec4Bufs[2].create(device, physicalDevice, mesh.vertexNormals.size() * sizeof(glm::vec4), mesh.vertexNormals.data());
    vec4Bufs[3].create(device, physicalDevice, mesh.faceNormals.size() * sizeof(glm::vec4), mesh.faceNormals.data());
    vec4Bufs[4].create(device, physicalDevice, mesh.faceCenters.size() * sizeof(glm::vec4), mesh.faceCenters.data());

    vec2Bufs[0].create(device, physicalDevice, mesh.vertexTexCoords.size() * sizeof(glm::vec2), mesh.vertexTexCoords.data());

    intBufs[0].create(device, physicalDevice, mesh.vertexEdges.size() * sizeof(int), mesh.vertexEdges.data());
    intBufs[1].create(device, physicalDevice, mesh.faceEdges.size() * sizeof(int), mesh.faceEdges.data());
    intBufs[2].create(device, physicalDevice, mesh.faceVertCounts.size() * sizeof(int), mesh.faceVertCounts.data());
    intBufs[3].create(device, physicalDevice, mesh.faceOffsets.size() * sizeof(int), mesh.faceOffsets.data());
    intBufs[4].create(device, physicalDevice, mesh.heVertex.size() * sizeof(int), mesh.heVertex.data());
    intBufs[5].create(device, physicalDevice, mesh.heFace.size() * sizeof(int), mesh.heFace.data());
    intBufs[6].create(device, physicalDevice, mesh.heNext.size() * sizeof(int), mesh.heNext.data());
    intBufs[7].create(device, physicalDevice, mesh.hePrev.size() * sizeof(int), mesh.hePrev.data());
    intBufs[8].create(device, physicalDevice, mesh.heTwin.size() * sizeof(int), mesh.heTwin.data());
    intBufs[9].create(device, physicalDevice, mesh.vertexFaceIndices.size() * sizeof(int), mesh.vertexFaceIndices.data());

    floatBufs[0].create(device, physicalDevice, mesh.faceAreas.size() * sizeof(float), mesh.faceAreas.data());

    MeshInfoUBO meshInfo{};
    meshInfo.nbVertices = mesh.nbVertices;
    meshInfo.nbFaces = mesh.nbFaces;
    meshInfo.nbHalfEdges = mesh.nbHalfEdges;
    meshInfo.slotsPerFace = 0;

    if (meshInfoBuf != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, meshInfoBuf, nullptr);
        vkFreeMemory(device, meshInfoMem, nullptr);
        meshInfoBuf = VK_NULL_HANDLE;
        meshInfoMem = VK_NULL_HANDLE;
    }
    createBuffer(sizeof(MeshInfoUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 meshInfoBuf, meshInfoMem);

    void* data;
    vkMapMemory(device, meshInfoMem, 0, sizeof(MeshInfoUBO), 0, &data);
    memcpy(data, &meshInfo, sizeof(MeshInfoUBO));
    vkUnmapMemory(device, meshInfoMem);
}

void Renderer::writeHEDescriptorSet(VkDescriptorSet dstSet,
                                      const std::vector<StorageBuffer>& vec4Bufs,
                                      const std::vector<StorageBuffer>& vec2Bufs,
                                      const std::vector<StorageBuffer>& intBufs,
                                      const std::vector<StorageBuffer>& floatBufs) {
    std::vector<VkWriteDescriptorSet> writes;

    std::vector<VkDescriptorBufferInfo> vec4Infos(5);
    for (int i = 0; i < 5; ++i) {
        vec4Infos[i].buffer = vec4Bufs[i].getBuffer();
        vec4Infos[i].offset = 0;
        vec4Infos[i].range = vec4Bufs[i].getSize();
    }
    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = dstSet;
    w.dstBinding = 0;
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.descriptorCount = 5;
    w.pBufferInfo = vec4Infos.data();
    writes.push_back(w);

    VkDescriptorBufferInfo vec2Info{};
    vec2Info.buffer = vec2Bufs[0].getBuffer();
    vec2Info.offset = 0;
    vec2Info.range = vec2Bufs[0].getSize();
    w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = dstSet;
    w.dstBinding = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo = &vec2Info;
    writes.push_back(w);

    std::vector<VkDescriptorBufferInfo> intInfos(10);
    for (int i = 0; i < 10; ++i) {
        intInfos[i].buffer = intBufs[i].getBuffer();
        intInfos[i].offset = 0;
        intInfos[i].range = intBufs[i].getSize();
    }
    w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = dstSet;
    w.dstBinding = 2;
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.descriptorCount = 10;
    w.pBufferInfo = intInfos.data();
    writes.push_back(w);

    VkDescriptorBufferInfo floatInfo{};
    floatInfo.buffer = floatBufs[0].getBuffer();
    floatInfo.offset = 0;
    floatInfo.range = floatBufs[0].getSize();
    w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = dstSet;
    w.dstBinding = 3;
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo = &floatInfo;
    writes.push_back(w);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

void Renderer::uploadHalfEdgeMesh(const HalfEdgeMesh& mesh) {
    std::cout << "Uploading half-edge mesh to GPU..." << std::endl;

    uploadHEBuffers(mesh, heVec4Buffers, heVec2Buffers, heIntBuffers, heFloatBuffers,
                    meshInfoBuffer, meshInfoMemory);
    writeHEDescriptorSet(heDescriptorSet, heVec4Buffers, heVec2Buffers, heIntBuffers, heFloatBuffers);

    // Store CPU copies for per-frame stats computation
    cpuFaceCenters.resize(mesh.nbFaces);
    cpuFaceNormals.resize(mesh.nbFaces);
    cpuFaceAreas = mesh.faceAreas;
    cpuFaceVertCounts = mesh.faceVertCounts;
    cpuVertexPositions.resize(mesh.nbVertices);
    cpuVertexNormals.resize(mesh.nbVertices);
    cpuVertexFaceAreas.resize(mesh.nbVertices);
    for (uint32_t i = 0; i < mesh.nbFaces; i++) {
        cpuFaceCenters[i] = glm::vec3(mesh.faceCenters[i]);
        cpuFaceNormals[i] = glm::vec3(mesh.faceNormals[i]);
    }
    cpuFaceUVs.resize(mesh.nbFaces);
    for (uint32_t i = 0; i < mesh.nbFaces; i++) {
        // Mirror GPU logic: face baseUV = texcoord of first vertex of face
        int edge = mesh.faceEdges[i];
        uint32_t firstVert = static_cast<uint32_t>(mesh.heVertex[edge]);
        cpuFaceUVs[i] = mesh.vertexTexCoords[firstVert];
    }
    cpuVertexUVs.resize(mesh.nbVertices);
    for (uint32_t i = 0; i < mesh.nbVertices; i++) {
        cpuVertexPositions[i] = glm::vec3(mesh.vertexPositions[i]);
        cpuVertexNormals[i]   = glm::vec3(mesh.vertexNormals[i]);
        int edge = mesh.vertexEdges[i];
        cpuVertexFaceAreas[i] = (edge >= 0) ? mesh.faceAreas[mesh.heFace[edge]] : 0.0f;
        cpuVertexUVs[i] = mesh.vertexTexCoords[i];
    }

    heMeshUploaded = true;
    visibleCacheDirty = true;
    heNbFaces = mesh.nbFaces;
    heNbVertices = mesh.nbVertices;
    heNbHalfEdges = mesh.nbHalfEdges;
    baseMeshTriCount = 0;
    for (uint32_t i = 0; i < mesh.nbFaces; i++)
        baseMeshTriCount += static_cast<uint32_t>(mesh.faceVertCounts[i]) - 2;

    size_t vram = calculateVRAM();
    std::cout << "Half-edge mesh uploaded to GPU" << std::endl;
    std::cout << "  Total VRAM: " << vram / 1024.0f << " KB" << std::endl;
}

void Renderer::updatePerObjectDescriptorSet() {
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = resurfacingUBOBuffer;
    uboInfo.offset = 0;
    uboInfo.range = sizeof(ResurfacingUBO);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = perObjectDescriptorSet;
    write.dstBinding = 0;  // BINDING_CONFIG_UBO
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &uboInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void Renderer::writeTextureDescriptors() {
    std::vector<VkWriteDescriptorSet> writes;

    // Write to both parametric and pebble descriptor sets
    VkDescriptorSet dstSets[] = { perObjectDescriptorSet, pebblePerObjectDescriptorSet };

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

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

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

void Renderer::cleanupMeshTextures() {
    aoTexture.destroy();
    elementTypeTexture.destroy();
    maskTexture.destroy();
    skinTexture.destroy();
    aoTextureLoaded = false;
    elementTypeTextureLoaded = false;
    maskTextureLoaded = false;
    skinTextureLoaded = false;
    dragonCoatAvailable = false;
    dragonCoatEnabled = false;
    dragonCoatPath.clear();
    useElementTypeTexture = false;
    useAOTexture = false;
    useMaskTexture = false;
    cpuMaskPixels.clear();
    cpuMaskWidth = 0;
    cpuMaskHeight = 0;
}

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

void Renderer::cleanupSecondaryMesh() {
    // Free descriptor sets before destroying the buffers they reference
    if (secondaryHeDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device, descriptorPool, 1, &secondaryHeDescriptorSet);
        secondaryHeDescriptorSet = VK_NULL_HANDLE;
    }
    if (secondaryPerObjectDescriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device, descriptorPool, 1, &secondaryPerObjectDescriptorSet);
        secondaryPerObjectDescriptorSet = VK_NULL_HANDLE;
    }
    for (auto& buf : secondaryHeVec4Buffers) buf.destroy();
    for (auto& buf : secondaryHeVec2Buffers) buf.destroy();
    for (auto& buf : secondaryHeIntBuffers) buf.destroy();
    for (auto& buf : secondaryHeFloatBuffers) buf.destroy();
    secondaryHeVec4Buffers.clear();
    secondaryHeVec2Buffers.clear();
    secondaryHeIntBuffers.clear();
    secondaryHeFloatBuffers.clear();
    if (secondaryMeshInfoBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, secondaryMeshInfoBuffer, nullptr);
        vkFreeMemory(device, secondaryMeshInfoMemory, nullptr);
        secondaryMeshInfoBuffer = VK_NULL_HANDLE;
        secondaryMeshInfoMemory = VK_NULL_HANDLE;
    }
    secondaryJointIndicesBuffer.destroy();
    secondaryJointWeightsBuffer.destroy();
    secondaryHeNbFaces = 0;
    secondaryHeNbVertices = 0;
    dualMeshActive = false;
}

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

void Renderer::loadSecondaryMesh(const std::string& path) {
    std::cout << "  Loading secondary mesh: " << path << std::endl;

    NGonMesh ngon = ObjLoader::load(path);
    HalfEdgeMesh mesh = HalfEdgeBuilder::build(ngon);
    computeFace2Coloring(mesh);

    secondaryHeNbFaces = mesh.nbFaces;
    secondaryHeNbVertices = mesh.nbVertices;

    // Upload half-edge buffers
    uploadHEBuffers(mesh, secondaryHeVec4Buffers, secondaryHeVec2Buffers,
                    secondaryHeIntBuffers, secondaryHeFloatBuffers,
                    secondaryMeshInfoBuffer, secondaryMeshInfoMemory);

    // Allocate secondary HE descriptor set (Set 1 layout)
    VkDescriptorSetAllocateInfo heAllocInfo{};
    heAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    heAllocInfo.descriptorPool = descriptorPool;
    heAllocInfo.descriptorSetCount = 1;
    heAllocInfo.pSetLayouts = &halfEdgeSetLayout;
    if (vkAllocateDescriptorSets(device, &heAllocInfo, &secondaryHeDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate secondary HE descriptor set!");
    }

    // Write secondary HE descriptor set
    writeHEDescriptorSet(secondaryHeDescriptorSet, secondaryHeVec4Buffers,
                         secondaryHeVec2Buffers, secondaryHeIntBuffers, secondaryHeFloatBuffers);

    // Allocate secondary per-object descriptor set (Set 2 layout)
    VkDescriptorSetAllocateInfo objAllocInfo{};
    objAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    objAllocInfo.descriptorPool = descriptorPool;
    objAllocInfo.descriptorSetCount = 1;
    objAllocInfo.pSetLayouts = &perObjectSetLayout;
    if (vkAllocateDescriptorSets(device, &objAllocInfo, &secondaryPerObjectDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate secondary per-object descriptor set!");
    }

    // Write binding 0: secondary ResurfacingUBO (independent from primary)
    {
        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = secondaryResurfacingUBOBuffer;
        uboInfo.offset = 0;
        uboInfo.range = sizeof(ResurfacingUBO);

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = secondaryPerObjectDescriptorSet;
        w.dstBinding = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &uboInfo;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }

    // Write binding 6: scale LUT (shared with primary — needed if secondary uses dragon scale)
    if (scaleLutBuffer.getBuffer() != VK_NULL_HANDLE) {
        VkDescriptorBufferInfo lutInfo{};
        lutInfo.buffer = scaleLutBuffer.getBuffer();
        lutInfo.offset = 0;
        lutInfo.range  = scaleLutBuffer.getSize();

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = secondaryPerObjectDescriptorSet;
        w.dstBinding = 6;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &lutInfo;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }

    // Match bone data for secondary mesh vertices (if skeleton loaded)
    if (skeletonLoaded) {
        // Re-use the already-loaded glTF model to match bone data to secondary mesh
        std::string baseName = path.substr(0, path.find_last_of('.'));
        std::string gltfPath = baseName + ".gltf";
        if (std::filesystem::exists(gltfPath)) {
            try {
                tinygltf::Model gltfModel = GltfLoader::loadModel(gltfPath);
                std::vector<glm::vec4> secJointIndices, secJointWeights;
                GltfLoader::matchBoneDataToObjMesh(gltfModel, ngon.positions,
                                                    skeleton, secJointIndices, secJointWeights);

                if (!secJointIndices.empty()) {
                    secondaryJointIndicesBuffer.create(device, physicalDevice,
                        secJointIndices.size() * sizeof(glm::vec4), secJointIndices.data());
                    secondaryJointWeightsBuffer.create(device, physicalDevice,
                        secJointWeights.size() * sizeof(glm::vec4), secJointWeights.data());

                    // Write skeleton bindings to secondary per-object descriptor set
                    std::vector<VkWriteDescriptorSet> writes;

                    VkDescriptorBufferInfo jointsInfo{};
                    jointsInfo.buffer = secondaryJointIndicesBuffer.getBuffer();
                    jointsInfo.offset = 0; jointsInfo.range = secondaryJointIndicesBuffer.getSize();
                    VkWriteDescriptorSet w{};
                    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    w.dstSet = secondaryPerObjectDescriptorSet;
                    w.dstBinding = 1; w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    w.descriptorCount = 1; w.pBufferInfo = &jointsInfo;
                    writes.push_back(w);

                    VkDescriptorBufferInfo weightsInfo{};
                    weightsInfo.buffer = secondaryJointWeightsBuffer.getBuffer();
                    weightsInfo.offset = 0; weightsInfo.range = secondaryJointWeightsBuffer.getSize();
                    w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    w.dstSet = secondaryPerObjectDescriptorSet;
                    w.dstBinding = 2; w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    w.descriptorCount = 1; w.pBufferInfo = &weightsInfo;
                    writes.push_back(w);

                    // Shared bone matrices buffer
                    VkDescriptorBufferInfo bonesInfo{};
                    bonesInfo.buffer = boneMatricesBuffer.getBuffer();
                    bonesInfo.offset = 0; bonesInfo.range = boneMatricesBuffer.getSize();
                    w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    w.dstSet = secondaryPerObjectDescriptorSet;
                    w.dstBinding = 3; w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    w.descriptorCount = 1; w.pBufferInfo = &bonesInfo;
                    writes.push_back(w);

                    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                                           writes.data(), 0, nullptr);

                    std::cout << "  Secondary mesh bone data matched: "
                              << secJointIndices.size() << " vertices" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "  Secondary mesh glTF error: " << e.what() << std::endl;
            }
        }
    }

    // Write samplers to secondary per-object descriptor set (shared with primary)
    if (linearSampler != VK_NULL_HANDLE) {
        VkDescriptorImageInfo samplerInfos[2] = {};
        samplerInfos[0].sampler = linearSampler;
        samplerInfos[1].sampler = nearestSampler;

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = secondaryPerObjectDescriptorSet;
        w.dstBinding = 4;
        w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        w.descriptorCount = 2;
        w.pImageInfo = samplerInfos;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }

    dualMeshActive = true;
    std::cout << "  Secondary mesh uploaded: " << mesh.nbFaces << " faces, "
              << mesh.nbVertices << " vertices" << std::endl;
}

void Renderer::generateGroundPlane(float cellSize) {
    // Compute grid resolution from desired world size.
    uint32_t N = static_cast<uint32_t>(std::ceil(groundWorldSize / cellSize));
    N = std::max(N, 4u);

    // On regeneration: destroy old GPU buffers but KEEP the descriptor set handles.
    // Re-allocating descriptor sets from the pool on every regeneration would exhaust
    // the pool's sampler/SSBO slots. Instead, just update the existing sets' writes
    // to point at the new buffers.

    // --- Free old GPU buffers ---
    for (auto& buf : groundHeVec4Buffers)  buf.destroy();
    for (auto& buf : groundHeVec2Buffers)  buf.destroy();
    for (auto& buf : groundHeIntBuffers)   buf.destroy();
    for (auto& buf : groundHeFloatBuffers) buf.destroy();
    groundHeVec4Buffers.clear();
    groundHeVec2Buffers.clear();
    groundHeIntBuffers.clear();
    groundHeFloatBuffers.clear();

    if (groundMeshInfoBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, groundMeshInfoBuffer, nullptr);
        vkFreeMemory(device, groundMeshInfoMemory, nullptr);
        groundMeshInfoBuffer = VK_NULL_HANDLE;
        groundMeshInfoMemory = VK_NULL_HANDLE;
    }
    if (groundPebbleUBOBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, groundPebbleUBOBuffer, nullptr);
        vkFreeMemory(device, groundPebbleUBOMemory, nullptr);
        groundPebbleUBOBuffer = VK_NULL_HANDLE;
        groundPebbleUBOMemory = VK_NULL_HANDLE;
        groundPebbleUBOMapped = nullptr;
    }

    // --- Build geometry ---
    NGonMesh ngon;

    if (groundMeshType == 0) {
        // ---- Quad grid ----
        uint32_t W = N + 1;
        ngon.nbVertices = W * W;
        ngon.nbFaces    = N * N;

        ngon.positions.resize(W * W);
        ngon.normals.resize(W * W, glm::vec3(0.0f, 1.0f, 0.0f));
        ngon.texCoords.resize(W * W, glm::vec2(0.0f));
        ngon.colors.resize(W * W, glm::vec3(1.0f));

        float halfSize = (N * cellSize) * 0.5f;
        for (uint32_t row = 0; row <= N; ++row) {
            for (uint32_t col = 0; col <= N; ++col) {
                uint32_t idx = row * W + col;
                ngon.positions[idx] = glm::vec3(col * cellSize - halfSize, 0.0f,
                                                 row * cellSize - halfSize);
                ngon.texCoords[idx] = glm::vec2(static_cast<float>(col) / N,
                                                 static_cast<float>(row) / N);
            }
        }

        uint32_t faceVertexOffset = 0;
        for (uint32_t row = 0; row < N; ++row) {
            for (uint32_t col = 0; col < N; ++col) {
                uint32_t v0 = row * W + col;
                uint32_t v1 = row * W + (col + 1);
                uint32_t v2 = (row + 1) * W + (col + 1);
                uint32_t v3 = (row + 1) * W + col;

                NGonFace face;
                face.vertexIndices = { v0, v1, v2, v3 };
                face.count  = 4;
                face.offset = faceVertexOffset;
                face.normal = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
                glm::vec3 c = (ngon.positions[v0] + ngon.positions[v1] +
                               ngon.positions[v2] + ngon.positions[v3]) * 0.25f;
                face.center = glm::vec4(c, 1.0f);
                face.area   = cellSize * cellSize;

                ngon.faces.push_back(std::move(face));
                ngon.faceVertexIndices.push_back(v0);
                ngon.faceVertexIndices.push_back(v1);
                ngon.faceVertexIndices.push_back(v2);
                ngon.faceVertexIndices.push_back(v3);
                faceVertexOffset += 4;
            }
        }
    } else {
        // ---- Pentagon tiling ----
        // Pairs of adjacent columns (2*pc, 2*pc+1) each become 2 pentagons sharing
        // one midpoint vertex placed at the CENTER X of the left cell (off the shared
        // edge), so the resulting faces are visually non-rectangular pentagons.
        // N is rounded up to even so all columns pair cleanly.
        if (N % 2 != 0) N += 1;

        uint32_t W         = N + 1;
        uint32_t numPairs  = N / 2;
        uint32_t numMidpts = numPairs * N;  // one midpoint per row per pair

        ngon.nbVertices = W * W + numMidpts;
        ngon.nbFaces    = N * N;

        ngon.positions.resize(ngon.nbVertices);
        ngon.normals.resize(ngon.nbVertices, glm::vec3(0.0f, 1.0f, 0.0f));
        ngon.texCoords.resize(ngon.nbVertices, glm::vec2(0.0f));
        ngon.colors.resize(ngon.nbVertices, glm::vec3(1.0f));

        float halfSize = (N * cellSize) * 0.5f;

        // Corner vertices
        for (uint32_t row = 0; row <= N; ++row) {
            for (uint32_t col = 0; col <= N; ++col) {
                uint32_t idx = row * W + col;
                ngon.positions[idx] = glm::vec3(col * cellSize - halfSize, 0.0f,
                                                 row * cellSize - halfSize);
                ngon.texCoords[idx] = glm::vec2(float(col) / N, float(row) / N);
            }
        }

        // Midpoint vertices: placed at the CENTER X of the left cell (not on the shared edge)
        // so that each pentagon is visually non-rectangular.
        auto midIdx = [&](uint32_t pc, uint32_t row) -> uint32_t {
            return W * W + pc * N + row;
        };
        for (uint32_t pc = 0; pc < numPairs; ++pc) {
            uint32_t col_right = 2 * pc + 1;
            float x = (col_right - 0.5f) * cellSize - halfSize;  // center of left cell
            for (uint32_t row = 0; row < N; ++row) {
                uint32_t idx = midIdx(pc, row);
                float z = (row + 0.5f) * cellSize - halfSize;
                ngon.positions[idx] = glm::vec3(x, 0.0f, z);
                ngon.texCoords[idx] = glm::vec2(float(col_right) / N,
                                                 float(row + 0.5f) / N);
            }
        }

        auto cornerIdx = [&](uint32_t row, uint32_t col) -> uint32_t {
            return row * W + col;
        };

        uint32_t faceVertexOffset = 0;
        for (uint32_t pc = 0; pc < numPairs; ++pc) {
            uint32_t col_l = 2 * pc;
            uint32_t col_r = 2 * pc + 1;
            for (uint32_t row = 0; row < N; ++row) {
                uint32_t M = midIdx(pc, row);

                // Pentagon 1 (left cell): TL → TR → midpoint → BR → BL
                {
                    uint32_t v0 = cornerIdx(row,   col_l);
                    uint32_t v1 = cornerIdx(row,   col_r);
                    uint32_t v2 = M;
                    uint32_t v3 = cornerIdx(row+1, col_r);
                    uint32_t v4 = cornerIdx(row+1, col_l);

                    glm::vec3 c = (ngon.positions[v0] + ngon.positions[v1] +
                                   ngon.positions[v2] + ngon.positions[v3] +
                                   ngon.positions[v4]) / 5.0f;
                    NGonFace face;
                    face.vertexIndices = { v0, v1, v2, v3, v4 };
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

                // Pentagon 2 (right cell): TL → TR → BR → BL → midpoint
                {
                    uint32_t v0 = cornerIdx(row,   col_r);
                    uint32_t v1 = cornerIdx(row,   col_r + 1);
                    uint32_t v2 = cornerIdx(row+1, col_r + 1);
                    uint32_t v3 = cornerIdx(row+1, col_r);
                    uint32_t v4 = M;

                    glm::vec3 c = (ngon.positions[v0] + ngon.positions[v1] +
                                   ngon.positions[v2] + ngon.positions[v3] +
                                   ngon.positions[v4]) / 5.0f;
                    NGonFace face;
                    face.vertexIndices = { v0, v1, v2, v3, v4 };
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

    HalfEdgeMesh mesh = HalfEdgeBuilder::build(ngon);
    computeFace2Coloring(mesh);
    groundNbFaces = mesh.nbFaces;

    // --- Upload new GPU buffers ---
    uploadHEBuffers(mesh, groundHeVec4Buffers, groundHeVec2Buffers,
                    groundHeIntBuffers, groundHeFloatBuffers,
                    groundMeshInfoBuffer, groundMeshInfoMemory);

    VkDeviceSize pebbleSize = sizeof(PebbleUBO);
    createBuffer(pebbleSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 groundPebbleUBOBuffer, groundPebbleUBOMemory);
    vkMapMemory(device, groundPebbleUBOMemory, 0, pebbleSize, 0, &groundPebbleUBOMapped);
    PebbleUBO initUBO{};
    memcpy(groundPebbleUBOMapped, &initUBO, sizeof(PebbleUBO));

    // --- Allocate descriptor sets only on first call; reuse on regeneration ---
    if (groundHeDescriptorSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo heAllocInfo{};
        heAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        heAllocInfo.descriptorPool = descriptorPool;
        heAllocInfo.descriptorSetCount = 1;
        heAllocInfo.pSetLayouts = &halfEdgeSetLayout;
        if (vkAllocateDescriptorSets(device, &heAllocInfo, &groundHeDescriptorSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate ground HE descriptor set!");
    }
    // Update HE set to point at the new buffers
    writeHEDescriptorSet(groundHeDescriptorSet, groundHeVec4Buffers,
                         groundHeVec2Buffers, groundHeIntBuffers, groundHeFloatBuffers);

    if (groundPebbleDescriptorSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo pebbleAllocInfo{};
        pebbleAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        pebbleAllocInfo.descriptorPool = descriptorPool;
        pebbleAllocInfo.descriptorSetCount = 1;
        pebbleAllocInfo.pSetLayouts = &perObjectSetLayout;
        if (vkAllocateDescriptorSets(device, &pebbleAllocInfo, &groundPebbleDescriptorSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate ground pebble descriptor set!");

        // Write samplers once (they never change)
        if (linearSampler != VK_NULL_HANDLE) {
            VkDescriptorImageInfo samplerInfos[2] = {};
            samplerInfos[0].sampler = linearSampler;
            samplerInfos[1].sampler = nearestSampler;

            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = groundPebbleDescriptorSet;
            w.dstBinding = 4;
            w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            w.descriptorCount = 2;
            w.pImageInfo = samplerInfos;
            vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
        }
    }
    // Always update UBO binding (new buffer handle each time)
    {
        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = groundPebbleUBOBuffer;
        uboInfo.offset = 0;
        uboInfo.range  = sizeof(PebbleUBO);

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = groundPebbleDescriptorSet;
        w.dstBinding = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &uboInfo;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }

    groundMeshActive = true;
    std::cout << "Ground plane generated: " << N << "x" << N
              << (groundMeshType == 0 ? " quads" : " pentagons")
              << " (" << groundNbFaces << " faces)" << std::endl;
}

void Renderer::cleanupGroundMesh() {
    for (auto& buf : groundHeVec4Buffers)  buf.destroy();
    for (auto& buf : groundHeVec2Buffers)  buf.destroy();
    for (auto& buf : groundHeIntBuffers)   buf.destroy();
    for (auto& buf : groundHeFloatBuffers) buf.destroy();
    groundHeVec4Buffers.clear();
    groundHeVec2Buffers.clear();
    groundHeIntBuffers.clear();
    groundHeFloatBuffers.clear();

    if (groundMeshInfoBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, groundMeshInfoBuffer, nullptr);
        vkFreeMemory(device, groundMeshInfoMemory, nullptr);
        groundMeshInfoBuffer = VK_NULL_HANDLE;
        groundMeshInfoMemory = VK_NULL_HANDLE;
    }

    if (groundPebbleUBOBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, groundPebbleUBOBuffer, nullptr);
        vkFreeMemory(device, groundPebbleUBOMemory, nullptr);
        groundPebbleUBOBuffer = VK_NULL_HANDLE;
        groundPebbleUBOMemory = VK_NULL_HANDLE;
        groundPebbleUBOMapped = nullptr;
    }

    groundHeDescriptorSet  = VK_NULL_HANDLE;
    groundPebbleDescriptorSet = VK_NULL_HANDLE;
    groundNbFaces    = 0;
    groundMeshActive = false;
}

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

void Renderer::loadScaleLut() {
    std::string path = std::string(ASSETS_DIR) + "parametric_luts/scale_lut.obj";
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "loadScaleLut: cannot open " << path << std::endl;
        return;
    }

    // Parse OBJ: collect positions, UVs, and face vertex pairs (posIdx, uvIdx)
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    // Use a set to deduplicate (posIdx, uvIdx) pairs from face entries
    std::vector<std::pair<int,int>> uniquePairs;
    std::set<std::pair<int,int>> seenPairs;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            positions.push_back({x, y, z});
        } else if (token == "vt") {
            float u, v;
            ss >> u >> v;
            uvs.push_back({u, v});
        } else if (token == "f") {
            // Each token: posIdx/uvIdx or posIdx/uvIdx/normalIdx
            std::string vert;
            while (ss >> vert) {
                // Parse first index (pos), second (uv)
                int vi = 0, vti = 0;
                char* ptr = vert.data();
                vi = std::strtol(ptr, &ptr, 10);
                if (*ptr == '/') {
                    ++ptr;
                    vti = std::strtol(ptr, &ptr, 10);
                }
                // OBJ indices are 1-based
                std::pair<int,int> key = {vi - 1, vti - 1};
                if (seenPairs.insert(key).second)
                    uniquePairs.push_back(key);
            }
        }
    }
    file.close();

    if (uniquePairs.empty() || uvs.empty() || positions.empty()) {
        std::cerr << "loadScaleLut: empty or malformed LUT file" << std::endl;
        return;
    }

    // Sort pairs by UV: primary = V ascending, secondary = U ascending
    std::sort(uniquePairs.begin(), uniquePairs.end(),
              [&](const std::pair<int,int>& a, const std::pair<int,int>& b) {
                  const glm::vec2& uvA = uvs[a.second];
                  const glm::vec2& uvB = uvs[b.second];
                  if (std::abs(uvA.y - uvB.y) > 1e-5f) return uvA.y < uvB.y;
                  return uvA.x < uvB.x;
              });

    // Compute grid dimensions: Nx = number of unique U values in first row
    uint32_t Nx = 0;
    float firstV = uvs[uniquePairs[0].second].y;
    for (auto& p : uniquePairs) {
        if (std::abs(uvs[p.second].y - firstV) < 1e-5f) Nx++;
        else break;
    }
    uint32_t Ny = static_cast<uint32_t>(uniquePairs.size()) / Nx;

    // Compute bounding extents
    glm::vec3 extMin(FLT_MAX), extMax(-FLT_MAX);
    for (auto& p : uniquePairs) {
        const glm::vec3& pos = positions[p.first];
        extMin = glm::min(extMin, pos);
        extMax = glm::max(extMax, pos);
    }

    // Pack as vec4[]
    std::vector<glm::vec4> packed;
    packed.reserve(uniquePairs.size());
    for (auto& p : uniquePairs)
        packed.push_back(glm::vec4(positions[p.first], 0.0f));

    // Upload to GPU (HOST_VISIBLE — buffer is tiny, ~2 KB)
    cleanupScaleLut();
    scaleLutBuffer.create(device, physicalDevice,
                          packed.size() * sizeof(glm::vec4),
                          packed.data());

    // Store LUT metadata as flat renderer member vars (picked up each frame by UBO upload)
    scaleLutNx        = Nx;
    scaleLutNy        = Ny;
    scaleLutMinExtent = glm::vec4(extMin, 0.0f);
    scaleLutMaxExtent = glm::vec4(extMax, 0.0f);
    scaleLutLoaded    = true;

    // Write descriptor (binding 6) for both per-object sets
    VkDescriptorBufferInfo lutInfo{};
    lutInfo.buffer = scaleLutBuffer.getBuffer();
    lutInfo.offset = 0;
    lutInfo.range  = VK_WHOLE_SIZE;

    VkDescriptorSet dstSets[] = { perObjectDescriptorSet, pebblePerObjectDescriptorSet };
    for (auto dstSet : dstSets) {
        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = dstSet;
        write.dstBinding      = 6;  // BINDING_SCALE_LUT
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &lutInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    std::cout << "Scale LUT loaded: " << Nx << "x" << Ny
              << " (" << uniquePairs.size() << " control points)" << std::endl;
}

void Renderer::cleanupScaleLut() {
    scaleLutBuffer.destroy();
    scaleLutLoaded = false;
}

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

    if (curvHdr.vertex_count != heNbVertices) {
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

    // --- Curvature (per-vertex, no remapping needed) ---
    {
        std::vector<float> curvature(curvHdr.vertex_count);
        std::ifstream f(curvPath, std::ios::binary);
        f.seekg(sizeof(PreprocessHeader));
        f.read(reinterpret_cast<char*>(curvature.data()), curvature.size() * sizeof(float));
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

void Renderer::loadAndUploadTexture(const std::string& path, VulkanTexture& texture,
                                     VkFormat format, bool& loadedFlag) {
    if (!std::filesystem::exists(path)) return;

    ImageData img = ImageLoader::load(path);
    if (img.pixels.empty()) return;

    texture.create(device, physicalDevice, img.width, img.height, format);
    texture.uploadData(commandPool, graphicsQueue, physicalDevice,
                       img.pixels.data(), img.pixels.size());
    loadedFlag = true;

    std::cout << "  Loaded texture: " << path
              << " (" << img.width << "x" << img.height << ")" << std::endl;
}

void Renderer::loadMesh(const std::string& path) {
    vkDeviceWaitIdle(device);

    // Cleanup previous mesh resources
    cleanupSecondaryMesh();
    cleanupMeshTextures();
    cleanupMeshSkeleton();
    cleanupGrwmPreprocess();

    loadedMeshPath = path;
    NGonMesh ngon = ObjLoader::load(path);
    if (triangulateMesh) {
        ObjLoader::triangulate(ngon);
    }
    if (subdivideLevel > 0) {
        ObjLoader::subdivide(ngon, subdivideLevel);
    }

    HalfEdgeMesh heMesh = HalfEdgeBuilder::build(ngon);
    computeFace2Coloring(heMesh);
    uploadHalfEdgeMesh(heMesh);

    // Load GRWM preprocessed data if available
    loadGrwmPreprocess(path);
    writeGrwmDescriptors(heDescriptorSet);

    // Auto-detect textures in the same directory as the mesh
    std::string dir = path.substr(0, path.find_last_of("/\\") + 1);

    // Determine AO texture name based on mesh filename
    std::string filename = path.substr(path.find_last_of("/\\") + 1);
    if (filename.find("dragon_coat") != std::string::npos) {
        loadAndUploadTexture(dir + "dragon_coat_ao.png", aoTexture,
                             VK_FORMAT_R8G8B8A8_SRGB, aoTextureLoaded);
    } else if (filename.find("dragon") != std::string::npos) {
        // Note: the AO file is named "dargon_ao.png" (typo in asset)
        loadAndUploadTexture(dir + "dargon_ao.png", aoTexture,
                             VK_FORMAT_R8G8B8A8_SRGB, aoTextureLoaded);
    }

    // Element type map (shared across dragon meshes)
    loadAndUploadTexture(dir + "dragon_element_type_map_2k.png", elementTypeTexture,
                         VK_FORMAT_R8G8B8A8_UNORM, elementTypeTextureLoaded);

    // Mask texture (per-face generation mask)
    loadAndUploadTexture(dir + "mask.png", maskTexture,
                         VK_FORMAT_R8G8B8A8_UNORM, maskTextureLoaded);
    if (maskTextureLoaded) {
        useMaskTexture = true;  // auto-enable
        // Keep CPU copy of mask R channel for stats
        ImageData maskImg = ImageLoader::load(dir + "mask.png");
        cpuMaskWidth = maskImg.width;
        cpuMaskHeight = maskImg.height;
        cpuMaskPixels.resize(maskImg.width * maskImg.height);
        for (uint32_t i = 0; i < maskImg.width * maskImg.height; i++)
            cpuMaskPixels[i] = maskImg.pixels[i * 4];  // R channel only
    }

    // Skin diffuse texture
    loadAndUploadTexture(dir + "skin.png", skinTexture,
                         VK_FORMAT_R8G8B8A8_SRGB, skinTextureLoaded);

    // Write sampler and texture descriptors if any textures were loaded
    if (aoTextureLoaded || elementTypeTextureLoaded || maskTextureLoaded || skinTextureLoaded) {
        writeTextureDescriptors();
    }

    // Auto-detect glTF skeleton (same name as OBJ, or any .gltf in same directory)
    std::string baseName = path.substr(0, path.find_last_of('.'));
    std::string gltfPath = baseName + ".gltf";
    if (!std::filesystem::exists(gltfPath)) {
        // Fallback: search directory for any .gltf file
        std::string gltfDir = path.substr(0, path.find_last_of("/\\") + 1);
        if (!gltfDir.empty()) {
            for (const auto& entry : std::filesystem::directory_iterator(gltfDir)) {
                if (entry.path().extension() == ".gltf") {
                    gltfPath = entry.path().string();
                    break;
                }
            }
        }
    }
    std::cout << "  glTF path check: " << gltfPath
              << " exists=" << std::filesystem::exists(gltfPath) << std::endl;
    if (std::filesystem::exists(gltfPath)) {
        std::cout << "  Loading glTF skeleton: " << gltfPath << std::endl;
        try {
            tinygltf::Model gltfModel = GltfLoader::loadModel(gltfPath);
            std::cout << "  glTF model loaded OK" << std::endl;
            GltfLoader::extractSkeleton(gltfModel, skeleton);
            std::cout << "  Skeleton extracted: " << skeleton.bones.size() << " bones" << std::endl;
            GltfLoader::extractAnimations(gltfModel, skeleton, animations);
            std::cout << "  Animations extracted: " << animations.size() << std::endl;
            GltfLoader::matchBoneDataToObjMesh(gltfModel, ngon.positions,
                                                skeleton, jointIndicesData, jointWeightsData);
            std::cout << "  Bone matching done" << std::endl;

            // Extract UVs from glTF and re-upload to GPU (OBJ has no UVs)
            std::vector<glm::vec2> gltfUVs;
            GltfLoader::matchUVsToObjMesh(gltfModel, ngon.positions, skeleton, gltfUVs);
            bool hasUVs = false;
            for (const auto& uv : gltfUVs) {
                if (uv.x != 0.0f || uv.y != 0.0f) { hasUVs = true; break; }
            }
            if (hasUVs) {
                heVec2Buffers[0].destroy();
                heVec2Buffers[0].create(device, physicalDevice,
                    gltfUVs.size() * sizeof(glm::vec2), gltfUVs.data());
                writeHEDescriptorSet(heDescriptorSet, heVec4Buffers, heVec2Buffers, heIntBuffers, heFloatBuffers);
                std::cout << "  UV buffer re-uploaded from glTF data" << std::endl;

                // Sync CPU UV arrays to match GPU (glTF UVs) so CPU pre-cull uses the same data
                for (uint32_t j = 0; j < static_cast<uint32_t>(gltfUVs.size()) && j < static_cast<uint32_t>(cpuVertexUVs.size()); j++)
                    cpuVertexUVs[j] = gltfUVs[j];
                for (uint32_t j = 0; j < heNbFaces; j++) {
                    int edge = heMesh.faceEdges[j];
                    uint32_t firstVert = static_cast<uint32_t>(heMesh.heVertex[edge]);
                    if (firstVert < gltfUVs.size())
                        cpuFaceUVs[j] = gltfUVs[firstVert];
                }
            }

            boneCount = static_cast<uint32_t>(skeleton.bones.size());
            std::cout << "  boneCount=" << boneCount
                      << " jointIndicesData.size()=" << jointIndicesData.size() << std::endl;
            if (boneCount > 0 && !jointIndicesData.empty()) {
                // Upload joint indices (device-local, static)
                jointIndicesBuffer.create(device, physicalDevice,
                    jointIndicesData.size() * sizeof(glm::vec4),
                    jointIndicesData.data());

                // Upload joint weights (device-local, static)
                jointWeightsBuffer.create(device, physicalDevice,
                    jointWeightsData.size() * sizeof(glm::vec4),
                    jointWeightsData.data());

                // Bone matrices buffer (already host-visible via StorageBuffer::create)
                std::vector<glm::mat4> boneMatrices;
                GltfLoader::computeBoneMatrices(skeleton, boneMatrices);
                boneMatricesBuffer.create(device, physicalDevice,
                    boneMatrices.size() * sizeof(glm::mat4),
                    boneMatrices.data());

                skeletonLoaded = true;
                writeSkeletonDescriptors();

                std::cout << "  Skeleton uploaded: " << boneCount << " bones, "
                          << jointIndicesData.size() << " skinned vertices" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "  glTF loading error: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "  glTF loading: unknown exception" << std::endl;
        }
    } else {
        std::cout << "  No glTF file found for skeleton" << std::endl;
    }

    // Check if a coat mesh exists alongside (e.g. dragon_coat.obj next to dragon.obj)
    {
        std::string coatPath = dir + "dragon_coat.obj";
        if (std::filesystem::exists(coatPath)) {
            dragonCoatAvailable = true;
            dragonCoatPath = coatPath;
        }
    }
}
