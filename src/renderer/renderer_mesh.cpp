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

    // Auto-detect glTF skeleton (same name as OBJ, with .gltf extension)
    std::string baseName = path.substr(0, path.find_last_of('.'));
    std::string gltfPath = baseName + ".gltf";
    if (std::filesystem::exists(gltfPath)) {
        std::cout << "  Loading glTF skeleton: " << gltfPath << std::endl;
        try {
            tinygltf::Model gltfModel = GltfLoader::loadModel(gltfPath);
            GltfLoader::extractSkeleton(gltfModel, skeleton);
            GltfLoader::extractAnimations(gltfModel, skeleton, animations);
            GltfLoader::matchBoneDataToObjMesh(gltfModel, ngon.positions,
                                                jointIndicesData, jointWeightsData);

            boneCount = static_cast<uint32_t>(skeleton.bones.size());
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
        }
    }
}
