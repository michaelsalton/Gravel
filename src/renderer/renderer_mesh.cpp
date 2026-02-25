#include "renderer/renderer.h"
#include "renderer/renderer_mesh.h"
#include "geometry/HalfEdge.h"
#include "loaders/ObjLoader.h"
#include "loaders/ImageLoader.h"
#include "loaders/GltfLoader.h"
#include <tiny_gltf.h>
#include "core/window.h"
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <filesystem>

void Renderer::uploadHalfEdgeMesh(const HalfEdgeMesh& mesh) {
    std::cout << "Uploading half-edge mesh to GPU..." << std::endl;

    heVec4Buffers.resize(5);
    heVec2Buffers.resize(1);
    heIntBuffers.resize(10);
    heFloatBuffers.resize(1);

    // vec4 buffers: positions, colors, normals, faceNormals, faceCenters
    heVec4Buffers[0].create(device, physicalDevice,
        mesh.vertexPositions.size() * sizeof(glm::vec4),
        mesh.vertexPositions.data());
    heVec4Buffers[1].create(device, physicalDevice,
        mesh.vertexColors.size() * sizeof(glm::vec4),
        mesh.vertexColors.data());
    heVec4Buffers[2].create(device, physicalDevice,
        mesh.vertexNormals.size() * sizeof(glm::vec4),
        mesh.vertexNormals.data());
    heVec4Buffers[3].create(device, physicalDevice,
        mesh.faceNormals.size() * sizeof(glm::vec4),
        mesh.faceNormals.data());
    heVec4Buffers[4].create(device, physicalDevice,
        mesh.faceCenters.size() * sizeof(glm::vec4),
        mesh.faceCenters.data());

    // vec2 buffer: texCoords
    heVec2Buffers[0].create(device, physicalDevice,
        mesh.vertexTexCoords.size() * sizeof(glm::vec2),
        mesh.vertexTexCoords.data());

    // int buffers: vertexEdges, faceEdges, faceVertCounts, faceOffsets,
    //              heVertex, heFace, heNext, hePrev, heTwin, vertexFaceIndices
    heIntBuffers[0].create(device, physicalDevice,
        mesh.vertexEdges.size() * sizeof(int), mesh.vertexEdges.data());
    heIntBuffers[1].create(device, physicalDevice,
        mesh.faceEdges.size() * sizeof(int), mesh.faceEdges.data());
    heIntBuffers[2].create(device, physicalDevice,
        mesh.faceVertCounts.size() * sizeof(int), mesh.faceVertCounts.data());
    heIntBuffers[3].create(device, physicalDevice,
        mesh.faceOffsets.size() * sizeof(int), mesh.faceOffsets.data());
    heIntBuffers[4].create(device, physicalDevice,
        mesh.heVertex.size() * sizeof(int), mesh.heVertex.data());
    heIntBuffers[5].create(device, physicalDevice,
        mesh.heFace.size() * sizeof(int), mesh.heFace.data());
    heIntBuffers[6].create(device, physicalDevice,
        mesh.heNext.size() * sizeof(int), mesh.heNext.data());
    heIntBuffers[7].create(device, physicalDevice,
        mesh.hePrev.size() * sizeof(int), mesh.hePrev.data());
    heIntBuffers[8].create(device, physicalDevice,
        mesh.heTwin.size() * sizeof(int), mesh.heTwin.data());
    heIntBuffers[9].create(device, physicalDevice,
        mesh.vertexFaceIndices.size() * sizeof(int), mesh.vertexFaceIndices.data());

    // float buffer: faceAreas
    heFloatBuffers[0].create(device, physicalDevice,
        mesh.faceAreas.size() * sizeof(float), mesh.faceAreas.data());

    // MeshInfo UBO
    MeshInfoUBO meshInfo{};
    meshInfo.nbVertices = mesh.nbVertices;
    meshInfo.nbFaces = mesh.nbFaces;
    meshInfo.nbHalfEdges = mesh.nbHalfEdges;
    meshInfo.padding = 0;

    createBuffer(sizeof(MeshInfoUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 meshInfoBuffer, meshInfoMemory);

    void* data;
    vkMapMemory(device, meshInfoMemory, 0, sizeof(MeshInfoUBO), 0, &data);
    memcpy(data, &meshInfo, sizeof(MeshInfoUBO));
    vkUnmapMemory(device, meshInfoMemory);

    updateHEDescriptorSet();

    // Store CPU copies for per-frame stats computation
    cpuFaceCenters.resize(mesh.nbFaces);
    cpuFaceNormals.resize(mesh.nbFaces);
    cpuFaceAreas = mesh.faceAreas;
    cpuVertexPositions.resize(mesh.nbVertices);
    cpuVertexNormals.resize(mesh.nbVertices);
    cpuVertexFaceAreas.resize(mesh.nbVertices);
    for (uint32_t i = 0; i < mesh.nbFaces; i++) {
        cpuFaceCenters[i] = glm::vec3(mesh.faceCenters[i]);
        cpuFaceNormals[i] = glm::vec3(mesh.faceNormals[i]);
    }
    for (uint32_t i = 0; i < mesh.nbVertices; i++) {
        cpuVertexPositions[i] = glm::vec3(mesh.vertexPositions[i]);
        cpuVertexNormals[i]   = glm::vec3(mesh.vertexNormals[i]);
        int edge = mesh.vertexEdges[i];
        cpuVertexFaceAreas[i] = (edge >= 0) ? mesh.faceAreas[mesh.heFace[edge]] : 0.0f;
    }

    heMeshUploaded = true;
    heNbFaces = mesh.nbFaces;
    heNbVertices = mesh.nbVertices;

    size_t vram = calculateVRAM();
    std::cout << "Half-edge mesh uploaded to GPU" << std::endl;
    std::cout << "  Total VRAM: " << vram / 1024.0f << " KB" << std::endl;
}

void Renderer::updateHEDescriptorSet() {
    std::vector<VkWriteDescriptorSet> writes;

    // Binding 0: vec4 buffers[5]
    std::vector<VkDescriptorBufferInfo> vec4BufferInfos(5);
    for (int i = 0; i < 5; ++i) {
        vec4BufferInfos[i].buffer = heVec4Buffers[i].getBuffer();
        vec4BufferInfos[i].offset = 0;
        vec4BufferInfos[i].range = heVec4Buffers[i].getSize();
    }

    VkWriteDescriptorSet vec4Write{};
    vec4Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vec4Write.dstSet = heDescriptorSet;
    vec4Write.dstBinding = 0;
    vec4Write.dstArrayElement = 0;
    vec4Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vec4Write.descriptorCount = 5;
    vec4Write.pBufferInfo = vec4BufferInfos.data();
    writes.push_back(vec4Write);

    // Binding 1: vec2 buffers[1]
    VkDescriptorBufferInfo vec2BufferInfo{};
    vec2BufferInfo.buffer = heVec2Buffers[0].getBuffer();
    vec2BufferInfo.offset = 0;
    vec2BufferInfo.range = heVec2Buffers[0].getSize();

    VkWriteDescriptorSet vec2Write{};
    vec2Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vec2Write.dstSet = heDescriptorSet;
    vec2Write.dstBinding = 1;
    vec2Write.dstArrayElement = 0;
    vec2Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vec2Write.descriptorCount = 1;
    vec2Write.pBufferInfo = &vec2BufferInfo;
    writes.push_back(vec2Write);

    // Binding 2: int buffers[10]
    std::vector<VkDescriptorBufferInfo> intBufferInfos(10);
    for (int i = 0; i < 10; ++i) {
        intBufferInfos[i].buffer = heIntBuffers[i].getBuffer();
        intBufferInfos[i].offset = 0;
        intBufferInfos[i].range = heIntBuffers[i].getSize();
    }

    VkWriteDescriptorSet intWrite{};
    intWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    intWrite.dstSet = heDescriptorSet;
    intWrite.dstBinding = 2;
    intWrite.dstArrayElement = 0;
    intWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    intWrite.descriptorCount = 10;
    intWrite.pBufferInfo = intBufferInfos.data();
    writes.push_back(intWrite);

    // Binding 3: float buffers[1]
    VkDescriptorBufferInfo floatBufferInfo{};
    floatBufferInfo.buffer = heFloatBuffers[0].getBuffer();
    floatBufferInfo.offset = 0;
    floatBufferInfo.range = heFloatBuffers[0].getSize();

    VkWriteDescriptorSet floatWrite{};
    floatWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    floatWrite.dstSet = heDescriptorSet;
    floatWrite.dstBinding = 3;
    floatWrite.dstArrayElement = 0;
    floatWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    floatWrite.descriptorCount = 1;
    floatWrite.pBufferInfo = &floatBufferInfo;
    writes.push_back(floatWrite);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
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

    // Binding 4: Samplers [linear, nearest]
    VkDescriptorImageInfo samplerInfos[2] = {};
    samplerInfos[0].sampler = linearSampler;
    samplerInfos[1].sampler = nearestSampler;

    VkWriteDescriptorSet samplerWrite{};
    samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    samplerWrite.dstSet = perObjectDescriptorSet;
    samplerWrite.dstBinding = 4;  // BINDING_SAMPLERS
    samplerWrite.dstArrayElement = 0;
    samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerWrite.descriptorCount = 2;
    samplerWrite.pImageInfo = samplerInfos;
    writes.push_back(samplerWrite);

    // Binding 5: Textures [AO, element type map]
    // Both slots must be written if either is loaded (partial binding is per-binding, not per-element)
    // Use a 1x1 placeholder for missing textures — but with partial binding we can write only loaded ones
    VkDescriptorImageInfo textureInfos[2] = {};

    // If AO is loaded, write slot 0
    if (aoTextureLoaded) {
        textureInfos[0].imageView = aoTexture.getImageView();
        textureInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet texWrite{};
        texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texWrite.dstSet = perObjectDescriptorSet;
        texWrite.dstBinding = 5;  // BINDING_TEXTURES
        texWrite.dstArrayElement = 0;  // AO_TEXTURE index
        texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texWrite.descriptorCount = 1;
        texWrite.pImageInfo = &textureInfos[0];
        writes.push_back(texWrite);
    }

    // If element type map is loaded, write slot 1
    if (elementTypeTextureLoaded) {
        textureInfos[1].imageView = elementTypeTexture.getImageView();
        textureInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet texWrite{};
        texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texWrite.dstSet = perObjectDescriptorSet;
        texWrite.dstBinding = 5;  // BINDING_TEXTURES
        texWrite.dstArrayElement = 1;  // ELEMENT_TYPE_TEXTURE index
        texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texWrite.descriptorCount = 1;
        texWrite.pImageInfo = &textureInfos[1];
        writes.push_back(texWrite);
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

void Renderer::processInput(Window& win, float deltaTime) {
    lastDeltaTime = deltaTime;
    camera.processInput(win, deltaTime);
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
    return total;
}

void Renderer::cleanupMeshTextures() {
    aoTexture.destroy();
    elementTypeTexture.destroy();
    aoTextureLoaded = false;
    elementTypeTextureLoaded = false;
    useElementTypeTexture = false;
    useAOTexture = false;
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
    secondaryHeDescriptorSet = VK_NULL_HANDLE;
    secondaryPerObjectDescriptorSet = VK_NULL_HANDLE;
    secondaryHeNbFaces = 0;
    secondaryHeNbVertices = 0;
    dualMeshActive = false;
}

void Renderer::loadSecondaryMesh(const std::string& path) {
    std::cout << "  Loading secondary mesh: " << path << std::endl;

    NGonMesh ngon = ObjLoader::load(path);
    HalfEdgeMesh mesh = HalfEdgeBuilder::build(ngon);
    computeFace2Coloring(mesh);

    secondaryHeNbFaces = mesh.nbFaces;
    secondaryHeNbVertices = mesh.nbVertices;

    // Upload half-edge buffers (mirrors uploadHalfEdgeMesh)
    secondaryHeVec4Buffers.resize(5);
    secondaryHeVec2Buffers.resize(1);
    secondaryHeIntBuffers.resize(10);
    secondaryHeFloatBuffers.resize(1);

    secondaryHeVec4Buffers[0].create(device, physicalDevice,
        mesh.vertexPositions.size() * sizeof(glm::vec4), mesh.vertexPositions.data());
    secondaryHeVec4Buffers[1].create(device, physicalDevice,
        mesh.vertexColors.size() * sizeof(glm::vec4), mesh.vertexColors.data());
    secondaryHeVec4Buffers[2].create(device, physicalDevice,
        mesh.vertexNormals.size() * sizeof(glm::vec4), mesh.vertexNormals.data());
    secondaryHeVec4Buffers[3].create(device, physicalDevice,
        mesh.faceNormals.size() * sizeof(glm::vec4), mesh.faceNormals.data());
    secondaryHeVec4Buffers[4].create(device, physicalDevice,
        mesh.faceCenters.size() * sizeof(glm::vec4), mesh.faceCenters.data());

    secondaryHeVec2Buffers[0].create(device, physicalDevice,
        mesh.vertexTexCoords.size() * sizeof(glm::vec2), mesh.vertexTexCoords.data());

    secondaryHeIntBuffers[0].create(device, physicalDevice,
        mesh.vertexEdges.size() * sizeof(int), mesh.vertexEdges.data());
    secondaryHeIntBuffers[1].create(device, physicalDevice,
        mesh.faceEdges.size() * sizeof(int), mesh.faceEdges.data());
    secondaryHeIntBuffers[2].create(device, physicalDevice,
        mesh.faceVertCounts.size() * sizeof(int), mesh.faceVertCounts.data());
    secondaryHeIntBuffers[3].create(device, physicalDevice,
        mesh.faceOffsets.size() * sizeof(int), mesh.faceOffsets.data());
    secondaryHeIntBuffers[4].create(device, physicalDevice,
        mesh.heVertex.size() * sizeof(int), mesh.heVertex.data());
    secondaryHeIntBuffers[5].create(device, physicalDevice,
        mesh.heFace.size() * sizeof(int), mesh.heFace.data());
    secondaryHeIntBuffers[6].create(device, physicalDevice,
        mesh.heNext.size() * sizeof(int), mesh.heNext.data());
    secondaryHeIntBuffers[7].create(device, physicalDevice,
        mesh.hePrev.size() * sizeof(int), mesh.hePrev.data());
    secondaryHeIntBuffers[8].create(device, physicalDevice,
        mesh.heTwin.size() * sizeof(int), mesh.heTwin.data());
    secondaryHeIntBuffers[9].create(device, physicalDevice,
        mesh.vertexFaceIndices.size() * sizeof(int), mesh.vertexFaceIndices.data());

    secondaryHeFloatBuffers[0].create(device, physicalDevice,
        mesh.faceAreas.size() * sizeof(float), mesh.faceAreas.data());

    // MeshInfo UBO for secondary mesh
    MeshInfoUBO meshInfo{};
    meshInfo.nbVertices = mesh.nbVertices;
    meshInfo.nbFaces = mesh.nbFaces;
    meshInfo.nbHalfEdges = mesh.nbHalfEdges;
    meshInfo.padding = 0;

    createBuffer(sizeof(MeshInfoUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 secondaryMeshInfoBuffer, secondaryMeshInfoMemory);

    void* data;
    vkMapMemory(device, secondaryMeshInfoMemory, 0, sizeof(MeshInfoUBO), 0, &data);
    memcpy(data, &meshInfo, sizeof(MeshInfoUBO));
    vkUnmapMemory(device, secondaryMeshInfoMemory);

    // Allocate secondary HE descriptor set (Set 1 layout)
    VkDescriptorSetAllocateInfo heAllocInfo{};
    heAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    heAllocInfo.descriptorPool = descriptorPool;
    heAllocInfo.descriptorSetCount = 1;
    heAllocInfo.pSetLayouts = &halfEdgeSetLayout;
    if (vkAllocateDescriptorSets(device, &heAllocInfo, &secondaryHeDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate secondary HE descriptor set!");
    }

    // Write secondary HE descriptor set (same pattern as updateHEDescriptorSet)
    {
        std::vector<VkWriteDescriptorSet> writes;

        std::vector<VkDescriptorBufferInfo> vec4Infos(5);
        for (int i = 0; i < 5; ++i) {
            vec4Infos[i].buffer = secondaryHeVec4Buffers[i].getBuffer();
            vec4Infos[i].offset = 0;
            vec4Infos[i].range = secondaryHeVec4Buffers[i].getSize();
        }
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = secondaryHeDescriptorSet;
        w.dstBinding = 0; w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.descriptorCount = 5; w.pBufferInfo = vec4Infos.data();
        writes.push_back(w);

        VkDescriptorBufferInfo vec2Info{};
        vec2Info.buffer = secondaryHeVec2Buffers[0].getBuffer();
        vec2Info.offset = 0; vec2Info.range = secondaryHeVec2Buffers[0].getSize();
        w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = secondaryHeDescriptorSet;
        w.dstBinding = 1; w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.descriptorCount = 1; w.pBufferInfo = &vec2Info;
        writes.push_back(w);

        std::vector<VkDescriptorBufferInfo> intInfos(10);
        for (int i = 0; i < 10; ++i) {
            intInfos[i].buffer = secondaryHeIntBuffers[i].getBuffer();
            intInfos[i].offset = 0;
            intInfos[i].range = secondaryHeIntBuffers[i].getSize();
        }
        w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = secondaryHeDescriptorSet;
        w.dstBinding = 2; w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.descriptorCount = 10; w.pBufferInfo = intInfos.data();
        writes.push_back(w);

        VkDescriptorBufferInfo floatInfo{};
        floatInfo.buffer = secondaryHeFloatBuffers[0].getBuffer();
        floatInfo.offset = 0; floatInfo.range = secondaryHeFloatBuffers[0].getSize();
        w = {}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = secondaryHeDescriptorSet;
        w.dstBinding = 3; w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.descriptorCount = 1; w.pBufferInfo = &floatInfo;
        writes.push_back(w);

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }

    // Allocate secondary per-object descriptor set (Set 2 layout)
    VkDescriptorSetAllocateInfo objAllocInfo{};
    objAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    objAllocInfo.descriptorPool = descriptorPool;
    objAllocInfo.descriptorSetCount = 1;
    objAllocInfo.pSetLayouts = &perObjectSetLayout;
    if (vkAllocateDescriptorSets(device, &objAllocInfo, &secondaryPerObjectDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate secondary per-object descriptor set!");
    }

    // Write binding 0: shared ResurfacingUBO
    {
        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = resurfacingUBOBuffer;
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

void Renderer::writeSkeletonDescriptors() {
    std::vector<VkWriteDescriptorSet> writes;

    // Binding 1: Joint indices SSBO
    VkDescriptorBufferInfo jointsInfo{};
    jointsInfo.buffer = jointIndicesBuffer.getBuffer();
    jointsInfo.offset = 0;
    jointsInfo.range = jointIndicesBuffer.getSize();

    VkWriteDescriptorSet jointsWrite{};
    jointsWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    jointsWrite.dstSet = perObjectDescriptorSet;
    jointsWrite.dstBinding = 1;  // BINDING_SKIN_JOINTS
    jointsWrite.dstArrayElement = 0;
    jointsWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    jointsWrite.descriptorCount = 1;
    jointsWrite.pBufferInfo = &jointsInfo;
    writes.push_back(jointsWrite);

    // Binding 2: Joint weights SSBO
    VkDescriptorBufferInfo weightsInfo{};
    weightsInfo.buffer = jointWeightsBuffer.getBuffer();
    weightsInfo.offset = 0;
    weightsInfo.range = jointWeightsBuffer.getSize();

    VkWriteDescriptorSet weightsWrite{};
    weightsWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    weightsWrite.dstSet = perObjectDescriptorSet;
    weightsWrite.dstBinding = 2;  // BINDING_SKIN_WEIGHTS
    weightsWrite.dstArrayElement = 0;
    weightsWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    weightsWrite.descriptorCount = 1;
    weightsWrite.pBufferInfo = &weightsInfo;
    writes.push_back(weightsWrite);

    // Binding 3: Bone matrices SSBO
    VkDescriptorBufferInfo bonesInfo{};
    bonesInfo.buffer = boneMatricesBuffer.getBuffer();
    bonesInfo.offset = 0;
    bonesInfo.range = boneMatricesBuffer.getSize();

    VkWriteDescriptorSet bonesWrite{};
    bonesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    bonesWrite.dstSet = perObjectDescriptorSet;
    bonesWrite.dstBinding = 3;  // BINDING_BONE_MATRICES
    bonesWrite.dstArrayElement = 0;
    bonesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bonesWrite.descriptorCount = 1;
    bonesWrite.pBufferInfo = &bonesInfo;
    writes.push_back(bonesWrite);

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

    NGonMesh ngon = ObjLoader::load(path);
    if (triangulateMesh) {
        ObjLoader::triangulate(ngon);
    }
    HalfEdgeMesh heMesh = HalfEdgeBuilder::build(ngon);
    computeFace2Coloring(heMesh);
    uploadHalfEdgeMesh(heMesh);

    // Auto-detect textures in the same directory as the mesh
    std::string dir = path.substr(0, path.find_last_of("/\\") + 1);

    // Determine AO texture name based on mesh filename
    std::string filename = path.substr(path.find_last_of("/\\") + 1);
    if (filename.find("dragon_coat") != std::string::npos) {
        loadAndUploadTexture(dir + "dragon_coat_ao.png", aoTexture,
                             VK_FORMAT_R8G8B8A8_SRGB, aoTextureLoaded);
    } else if (filename.find("dragon") != std::string::npos) {
        // Note: the AO file is named "dargon_8k_ao.png" (typo in asset)
        loadAndUploadTexture(dir + "dargon_8k_ao.png", aoTexture,
                             VK_FORMAT_R8G8B8A8_SRGB, aoTextureLoaded);
    }

    // Element type map (shared across dragon meshes)
    loadAndUploadTexture(dir + "dragon_element_type_map_2k.png", elementTypeTexture,
                         VK_FORMAT_R8G8B8A8_UNORM, elementTypeTextureLoaded);

    // Write sampler and texture descriptors if any textures were loaded
    if (aoTextureLoaded || elementTypeTextureLoaded) {
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

    // Dragon Coat: also load base dragon mesh for solid rendering underneath
    if (filename.find("dragon_coat") != std::string::npos) {
        loadSecondaryMesh(dir + "dragon_8k.obj");
    }
}
