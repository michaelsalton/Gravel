#include "renderer/renderer.h"
#include "renderer/renderer_mesh.h"
#include "geometry/HalfEdge.h"
#include "loaders/ObjLoader.h"
#include "core/window.h"
#include <iostream>
#include <cstring>
#include <stdexcept>

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

    VkDescriptorBufferInfo ssboInfo{};
    ssboInfo.buffer = lutSSBOBuffer;
    ssboInfo.offset = 0;
    ssboInfo.range = VK_WHOLE_SIZE;

    std::array<VkWriteDescriptorSet, 2> writes{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = perObjectDescriptorSet;
    writes[0].dstBinding = 0;  // BINDING_CONFIG_UBO
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &uboInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = perObjectDescriptorSet;
    writes[1].dstBinding = 2;  // BINDING_LUT_BUFFER
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &ssboInfo;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

void Renderer::processInput(Window& win, float deltaTime) {
    camera.processInput(win, deltaTime);
}

void Renderer::loadControlCage(const std::string& filepath) {
    LutData lut = LUTLoader::loadControlCage(filepath);
    if (!lut.isValid) {
        std::cerr << "LUT: failed to load " << filepath << std::endl;
        return;
    }

    // Wait for GPU to finish using the old LUT buffer before replacing it
    vkDeviceWaitIdle(device);

    // Destroy old LUT SSBO
    if (lutSSBOBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, lutSSBOBuffer, nullptr);
        vkFreeMemory(device, lutSSBOMemory, nullptr);
        lutSSBOBuffer = VK_NULL_HANDLE;
        lutSSBOMemory = VK_NULL_HANDLE;
    }

    // Upload control points as vec4 array (w = 1.0 padding)
    std::vector<glm::vec4> data;
    data.reserve(lut.controlPoints.size());
    for (const auto& p : lut.controlPoints)
        data.push_back(glm::vec4(p, 1.0f));

    lutSSBOSize = data.size() * sizeof(glm::vec4);
    createBuffer(lutSSBOSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 lutSSBOBuffer, lutSSBOMemory);

    void* mapped;
    vkMapMemory(device, lutSSBOMemory, 0, lutSSBOSize, 0, &mapped);
    memcpy(mapped, data.data(), lutSSBOSize);
    vkUnmapMemory(device, lutSSBOMemory);

    // Update ResurfacingUBO with LUT metadata
    ResurfacingUBO* resurfData = static_cast<ResurfacingUBO*>(resurfacingUBOMapped);
    resurfData->lutNx    = lut.Nx;
    resurfData->lutNy    = lut.Ny;
    resurfData->cyclicU  = cyclicU ? 1u : 0u;
    resurfData->cyclicV  = cyclicV ? 1u : 0u;
    resurfData->lutBBMin = glm::vec4(lut.bbMin, 0.0f);
    resurfData->lutBBMax = glm::vec4(lut.bbMax, 0.0f);

    currentLut  = lut;
    lutLoaded   = true;
    lutFilename = filepath;

    // Rebind descriptor set with new SSBO
    updatePerObjectDescriptorSet();

    std::cout << "LUT loaded and uploaded: " << lut.controlPoints.size()
              << " control points (" << lut.Nx << "x" << lut.Ny << ")" << std::endl;
}

size_t Renderer::calculateVRAM() const {
    size_t total = 0;
    for (const auto& buf : heVec4Buffers) total += buf.getSize();
    for (const auto& buf : heVec2Buffers) total += buf.getSize();
    for (const auto& buf : heIntBuffers) total += buf.getSize();
    for (const auto& buf : heFloatBuffers) total += buf.getSize();
    total += sizeof(MeshInfoUBO);
    return total;
}

void Renderer::loadMesh(const std::string& path) {
    vkDeviceWaitIdle(device);
    NGonMesh ngon = ObjLoader::load(path);
    HalfEdgeMesh heMesh = HalfEdgeBuilder::build(ngon);
    uploadHalfEdgeMesh(heMesh);
}
