#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE        // Already defined in ImageLoader.cpp
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include "loaders/GltfLoader.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <set>

// ============================================================================
// Helper functions for node transforms
// ============================================================================

static glm::vec3 getNodeTranslation(const tinygltf::Node& node) {
    if (!node.translation.empty()) {
        return glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2]));
    }
    return glm::vec3(0.0f);
}

static glm::quat getNodeRotation(const tinygltf::Node& node) {
    if (!node.rotation.empty()) {
        // glTF quaternion: [x, y, z, w] -> glm::quat(w, x, y, z)
        return glm::quat(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2]));
    }
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

static glm::vec3 getNodeScale(const tinygltf::Node& node) {
    if (!node.scale.empty()) {
        return glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2]));
    }
    return glm::vec3(1.0f);
}

static glm::mat4 getNodeTransform(const tinygltf::Node& node) {
    glm::mat4 T = glm::translate(glm::mat4(1.0f), getNodeTranslation(node));
    glm::mat4 R = glm::mat4_cast(getNodeRotation(node));
    glm::mat4 S = glm::scale(glm::mat4(1.0f), getNodeScale(node));
    return T * R * S;
}

// ============================================================================
// GltfLoader implementation
// ============================================================================

// No-op image loader: we only need skeleton/animation data from glTF
static bool dummyLoadImageData(tinygltf::Image*, const int, std::string*,
                                std::string*, int, int,
                                const unsigned char*, int, void*) {
    return true;  // pretend success, skip actual image loading
}

tinygltf::Model GltfLoader::loadModel(const std::string& filepath) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string error, warning;

    loader.SetImageLoader(dummyLoadImageData, nullptr);
    bool loaded = loader.LoadASCIIFromFile(&model, &error, &warning, filepath);

    if (!warning.empty()) {
        std::cerr << "glTF Warning: " << warning << std::endl;
    }
    if (!error.empty()) {
        std::cerr << "glTF Error: " << error << std::endl;
    }
    if (!loaded) {
        throw std::runtime_error("Failed to load glTF model: " + filepath);
    }

    return model;
}

void GltfLoader::extractSkeleton(const tinygltf::Model& model, Skeleton& skeleton) {
    if (model.skins.empty()) {
        std::cerr << "No skins found in the glTF model." << std::endl;
        return;
    }

    const tinygltf::Skin& skin = model.skins[0];

    // Load inverse bind matrices
    std::vector<glm::mat4> inverseBindMatrices;
    if (skin.inverseBindMatrices >= 0) {
        const tinygltf::Accessor& accessor = model.accessors[skin.inverseBindMatrices];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        const unsigned char* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

        size_t numMatrices = accessor.count;
        inverseBindMatrices.resize(numMatrices);
        memcpy(inverseBindMatrices.data(), dataPtr, sizeof(glm::mat4) * numMatrices);
    }

    // Build the bone hierarchy
    skeleton.bones.resize(skin.joints.size());
    for (size_t i = 0; i < skin.joints.size(); ++i) {
        int nodeIndex = skin.joints[i];
        const tinygltf::Node& node = model.nodes[nodeIndex];

        Skeleton::Bone& bone = skeleton.bones[i];
        bone.nodeIndex = nodeIndex;
        bone.name = node.name;
        bone.parentIndex = -1;

        // Set inverse bind matrix
        bone.inverseBindMatrix = inverseBindMatrices.empty()
            ? glm::mat4(1.0f) : inverseBindMatrices[i];

        // Initialize animation data from node's rest pose
        bone.animTranslation = getNodeTranslation(node);
        bone.animRotation = getNodeRotation(node);
        bone.animScale = getNodeScale(node);
        bone.localTransform = getNodeTransform(node);

        // Find parent bone by checking which joint node lists this node as a child
        for (size_t j = 0; j < skin.joints.size(); ++j) {
            const tinygltf::Node& parentNode = model.nodes[skin.joints[j]];
            if (std::find(parentNode.children.begin(), parentNode.children.end(),
                          nodeIndex) != parentNode.children.end()) {
                bone.parentIndex = static_cast<int>(j);
                skeleton.bones[j].childrenIndices.push_back(static_cast<int>(i));
                break;
            }
        }
    }

    // Find the armature (skeleton root) node that parents the joint nodes.
    // Check skin.skeleton first, otherwise search all nodes for the common parent.
    skeleton.armatureTransform = glm::mat4(1.0f);
    if (skin.skeleton >= 0) {
        skeleton.armatureTransform = getNodeTransform(model.nodes[skin.skeleton]);
    } else {
        // Find the non-joint node that has root bones as children
        std::set<int> jointSet(skin.joints.begin(), skin.joints.end());
        for (size_t n = 0; n < model.nodes.size(); ++n) {
            if (jointSet.count(static_cast<int>(n))) continue;  // skip joint nodes
            const tinygltf::Node& node = model.nodes[n];
            for (int child : node.children) {
                if (jointSet.count(child)) {
                    skeleton.armatureTransform = getNodeTransform(node);
                    goto foundArmature;
                }
            }
        }
        foundArmature:;
    }

    std::cout << "  Skeleton: " << skeleton.bones.size() << " bones" << std::endl;
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        std::cout << "    [" << i << "] " << skeleton.bones[i].name
                  << " (parent=" << skeleton.bones[i].parentIndex << ")" << std::endl;
    }
}

// Recursive helper to compute global transforms
static void computeGlobalTransform(const Skeleton& skeleton, int boneIndex,
                                    std::vector<glm::mat4>& globalTransforms) {
    const Skeleton::Bone& bone = skeleton.bones[boneIndex];
    glm::mat4 localTransform = bone.localTransform;

    if (bone.parentIndex != -1) {
        computeGlobalTransform(skeleton, bone.parentIndex, globalTransforms);
        globalTransforms[boneIndex] = globalTransforms[bone.parentIndex] * localTransform;
    } else {
        // Root bones: apply armature (skeleton root) transform
        globalTransforms[boneIndex] = skeleton.armatureTransform * localTransform;
    }
}

void GltfLoader::computeBoneMatrices(const Skeleton& skeleton,
                                      std::vector<glm::mat4>& boneMatrices) {
    boneMatrices.resize(skeleton.bones.size());
    std::vector<glm::mat4> globalTransforms(skeleton.bones.size());

    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        computeGlobalTransform(skeleton, static_cast<int>(i), globalTransforms);
        boneMatrices[i] = globalTransforms[i] * skeleton.bones[i].inverseBindMatrix;
    }

    // Conjugate bone matrices by the OBJ alignment transform so that
    // bone pivots are in OBJ coordinate space instead of glTF mesh-local space.
    // boneMatrix_obj = T_align * boneMatrix_gltf * inverse(T_align)
    if (skeleton.objAlignTransform != glm::mat4(1.0f)) {
        for (size_t i = 0; i < boneMatrices.size(); ++i) {
            boneMatrices[i] = skeleton.objAlignTransform * boneMatrices[i] * skeleton.objAlignInverse;
        }
    }
}

void GltfLoader::extractAnimations(const tinygltf::Model& model,
                                    const Skeleton& skeleton,
                                    std::vector<Animation>& animations) {
    for (const auto& gltfAnimation : model.animations) {
        Animation animation;
        animation.name = gltfAnimation.name;
        animation.duration = 0.0f;

        for (const auto& channel : gltfAnimation.channels) {
            const tinygltf::AnimationSampler& sampler = gltfAnimation.samplers[channel.sampler];
            std::string interpolation = sampler.interpolation.empty()
                ? "LINEAR" : sampler.interpolation;

            // Load keyframe times
            std::vector<float> times;
            {
                const tinygltf::Accessor& accessor = model.accessors[sampler.input];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const unsigned char* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                times.resize(accessor.count);
                memcpy(times.data(), dataPtr, sizeof(float) * accessor.count);
            }

            // Load keyframe values
            std::vector<float> values;
            {
                const tinygltf::Accessor& accessor = model.accessors[sampler.output];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const unsigned char* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                size_t count = accessor.count * tinygltf::GetNumComponentsInType(accessor.type);
                values.resize(count);
                memcpy(values.data(), dataPtr, sizeof(float) * count);
            }

            // Find the corresponding bone
            int boneIndex = -1;
            for (size_t i = 0; i < skeleton.bones.size(); ++i) {
                if (skeleton.bones[i].nodeIndex == channel.target_node) {
                    boneIndex = static_cast<int>(i);
                    break;
                }
            }
            if (boneIndex == -1) continue;

            AnimationChannel animChannel;
            animChannel.boneIndex = boneIndex;
            animChannel.path = channel.target_path;
            animChannel.interpolation = interpolation;

            // Determine value components per keyframe
            size_t valueComponents = 0;
            if (channel.target_path == "translation" || channel.target_path == "scale") {
                valueComponents = 3;
            } else if (channel.target_path == "rotation") {
                valueComponents = 4;
            } else {
                continue; // Unsupported path
            }

            // Create keyframes
            for (size_t i = 0; i < times.size(); ++i) {
                KeyFrame keyframe;
                keyframe.time = times[i];
                if (keyframe.time > animation.duration) {
                    animation.duration = keyframe.time;
                }

                size_t offset = i * valueComponents;

                if (channel.target_path == "translation") {
                    keyframe.translation = glm::make_vec3(&values[offset]);
                } else if (channel.target_path == "rotation") {
                    keyframe.rotation = glm::make_quat(&values[offset]);
                } else if (channel.target_path == "scale") {
                    keyframe.scale = glm::make_vec3(&values[offset]);
                }

                animChannel.keyframes.push_back(keyframe);
            }

            animation.channels.push_back(animChannel);
        }

        animations.push_back(animation);
        std::cout << "  Animation: \"" << animation.name << "\" duration="
                  << animation.duration << "s, " << animation.channels.size()
                  << " channels" << std::endl;
    }
}

void GltfLoader::updateSkeleton(const Animation& animation, float time,
                                 Skeleton& skeleton) {
    for (const auto& channel : animation.channels) {
        int boneIndex = channel.boneIndex;
        Skeleton::Bone& bone = skeleton.bones[boneIndex];

        const auto& keyframes = channel.keyframes;
        size_t numKeyframes = keyframes.size();
        if (numKeyframes == 0) continue;

        // Handle time outside the animation range
        float t_clamped = time;
        if (t_clamped < keyframes.front().time) {
            t_clamped = keyframes.front().time;
        } else if (t_clamped > keyframes.back().time) {
            t_clamped = std::fmod(t_clamped, animation.duration);
        }

        // Find the surrounding keyframe pair
        size_t kfIndex = 0;
        for (; kfIndex < numKeyframes - 1; ++kfIndex) {
            if (t_clamped < keyframes[kfIndex + 1].time) break;
        }

        const KeyFrame& kf0 = keyframes[kfIndex];
        const KeyFrame& kf1 = keyframes[std::min(kfIndex + 1, numKeyframes - 1)];

        float t = 0.0f;
        if (kf0.time != kf1.time) {
            t = (t_clamped - kf0.time) / (kf1.time - kf0.time);
        }

        // Interpolate
        if (channel.interpolation == "LINEAR") {
            if (channel.path == "translation") {
                bone.animTranslation = glm::mix(kf0.translation, kf1.translation, t);
            } else if (channel.path == "rotation") {
                bone.animRotation = glm::slerp(kf0.rotation, kf1.rotation, t);
            } else if (channel.path == "scale") {
                bone.animScale = glm::mix(kf0.scale, kf1.scale, t);
            }
        } else if (channel.interpolation == "STEP") {
            if (channel.path == "translation") {
                bone.animTranslation = kf0.translation;
            } else if (channel.path == "rotation") {
                bone.animRotation = kf0.rotation;
            } else if (channel.path == "scale") {
                bone.animScale = kf0.scale;
            }
        }

        // Reconstruct local transform from TRS
        bone.localTransform = glm::translate(glm::mat4(1.0f), bone.animTranslation) *
                              glm::mat4_cast(bone.animRotation) *
                              glm::scale(glm::mat4(1.0f), bone.animScale);
    }
}

void GltfLoader::matchBoneDataToObjMesh(const tinygltf::Model& model,
                                         const std::vector<glm::vec3>& objPositions,
                                         Skeleton& skeleton,
                                         std::vector<glm::vec4>& jointIndices,
                                         std::vector<glm::vec4>& jointWeights) {
    size_t objVertCount = objPositions.size();
    jointIndices.assign(objVertCount, glm::vec4(0.0f));
    jointWeights.assign(objVertCount, glm::vec4(0.0f));

    // Collect all glTF vertex data
    struct GltfVertData {
        glm::vec3 pos;
        size_t primitiveIdx;
        size_t vertexIdx;
    };
    std::vector<GltfVertData> gltfVerts;

    struct PrimitiveData {
        const uint8_t* jointData;
        const float* weightData;
        int jointComponentType;
    };
    std::vector<PrimitiveData> primitives;

    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            const auto& attributes = primitive.attributes;
            auto jointsIt = attributes.find("JOINTS_0");
            auto weightsIt = attributes.find("WEIGHTS_0");
            auto posIt = attributes.find("POSITION");

            if (jointsIt == attributes.end() || weightsIt == attributes.end() ||
                posIt == attributes.end()) {
                std::cerr << "JOINTS_0, WEIGHTS_0, or POSITION not found in glTF mesh." << std::endl;
                continue;
            }

            const tinygltf::Accessor& jointAccessor = model.accessors[jointsIt->second];
            const tinygltf::Accessor& weightAccessor = model.accessors[weightsIt->second];
            const tinygltf::Accessor& posAccessor = model.accessors[posIt->second];

            const tinygltf::BufferView& jointBV = model.bufferViews[jointAccessor.bufferView];
            const tinygltf::BufferView& weightBV = model.bufferViews[weightAccessor.bufferView];
            const tinygltf::BufferView& posBV = model.bufferViews[posAccessor.bufferView];

            const uint8_t* jointRaw = reinterpret_cast<const uint8_t*>(
                &model.buffers[jointBV.buffer].data[jointAccessor.byteOffset + jointBV.byteOffset]);
            const float* weightData = reinterpret_cast<const float*>(
                &model.buffers[weightBV.buffer].data[weightAccessor.byteOffset + weightBV.byteOffset]);
            const float* posData = reinterpret_cast<const float*>(
                &model.buffers[posBV.buffer].data[posAccessor.byteOffset + posBV.byteOffset]);

            size_t primIdx = primitives.size();
            primitives.push_back({ jointRaw, weightData, jointAccessor.componentType });

            size_t gltfVertCount = posAccessor.count;
            for (size_t i = 0; i < gltfVertCount; ++i) {
                gltfVerts.push_back({
                    glm::vec3(posData[i * 3 + 0], posData[i * 3 + 1], posData[i * 3 + 2]),
                    primIdx, i
                });
            }
        }
    }

    if (gltfVerts.empty()) {
        std::cerr << "  No glTF vertices found for bone matching." << std::endl;
        return;
    }

    // Auto-align coordinate spaces: detect scale/offset between OBJ and glTF AABBs
    glm::vec3 objMin(std::numeric_limits<float>::max());
    glm::vec3 objMax(std::numeric_limits<float>::lowest());
    for (size_t j = 0; j < objVertCount; ++j) {
        objMin = glm::min(objMin, objPositions[j]);
        objMax = glm::max(objMax, objPositions[j]);
    }
    glm::vec3 gltfMin(std::numeric_limits<float>::max());
    glm::vec3 gltfMax(std::numeric_limits<float>::lowest());
    for (const auto& gv : gltfVerts) {
        gltfMin = glm::min(gltfMin, gv.pos);
        gltfMax = glm::max(gltfMax, gv.pos);
    }

    glm::vec3 objExtent = objMax - objMin;
    glm::vec3 gltfExtent = gltfMax - gltfMin;
    glm::vec3 objCenter = (objMin + objMax) * 0.5f;
    glm::vec3 gltfCenter = (gltfMin + gltfMax) * 0.5f;

    // Compute uniform scale using the largest axis (most reliable)
    float scale = 1.0f;
    int bestAxis = 0;
    float bestExtent = 0.0f;
    for (int a = 0; a < 3; ++a) {
        if (gltfExtent[a] > bestExtent) {
            bestExtent = gltfExtent[a];
            bestAxis = a;
        }
    }
    if (gltfExtent[bestAxis] > 1e-6f) {
        scale = objExtent[bestAxis] / gltfExtent[bestAxis];
    }
    glm::vec3 offset = objCenter - scale * gltfCenter;

    std::cout << "  Bone matching: OBJ extent=(" << objExtent.x << "," << objExtent.y << "," << objExtent.z << ")"
              << " glTF extent=(" << gltfExtent.x << "," << gltfExtent.y << "," << gltfExtent.z << ")"
              << " scale=" << scale << std::endl;

    // Store alignment transform in skeleton for bone matrix conjugation.
    // T_align transforms glTF mesh-local positions to OBJ space: p_obj = scale * p_gltf + offset
    skeleton.objAlignTransform = glm::translate(glm::mat4(1.0f), offset) *
                                  glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    skeleton.objAlignInverse = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / scale)) *
                                glm::translate(glm::mat4(1.0f), -offset);

    // Transform glTF positions to OBJ space
    std::vector<glm::vec3> alignedGltfPos(gltfVerts.size());
    for (size_t i = 0; i < gltfVerts.size(); ++i) {
        alignedGltfPos[i] = scale * gltfVerts[i].pos + offset;
    }

    // Adaptive epsilon from sampled nearest-neighbor distances
    float epsilon = 1e-5f;
    {
        size_t sampleCount = std::min(size_t(200), gltfVerts.size());
        size_t step = std::max(size_t(1), gltfVerts.size() / sampleCount);
        std::vector<float> nnDists;
        nnDists.reserve(sampleCount);

        for (size_t s = 0; s < gltfVerts.size() && nnDists.size() < sampleCount; s += step) {
            float bestDist = std::numeric_limits<float>::max();
            for (size_t j = 0; j < objVertCount; ++j) {
                float d = glm::distance(alignedGltfPos[s], objPositions[j]);
                if (d < bestDist) bestDist = d;
            }
            float maxReasonable = objExtent[bestAxis] * 0.1f;  // 10% of mesh size
            if (bestDist < maxReasonable) {
                nnDists.push_back(bestDist);
            }
        }

        if (!nnDists.empty()) {
            std::sort(nnDists.begin(), nnDists.end());
            float median = nnDists[nnDists.size() / 2];
            epsilon = std::clamp(median * 3.0f, 1e-5f, objExtent[bestAxis] * 0.02f);
        }

        std::cout << "  Bone matching epsilon: " << epsilon << std::endl;
    }

    float cellSize = std::max(epsilon * 3.0f, 1e-4f);

    // Spatial hash structs
    struct CellKey {
        int x, y, z;
        bool operator==(const CellKey& o) const { return x == o.x && y == o.y && z == o.z; }
    };
    struct CellHash {
        size_t operator()(const CellKey& k) const {
            return size_t(k.x * 73856093) ^ size_t(k.y * 19349663) ^ size_t(k.z * 83492791);
        }
    };

    auto toCell = [cellSize](const glm::vec3& p) -> CellKey {
        return { static_cast<int>(std::floor(p.x / cellSize)),
                 static_cast<int>(std::floor(p.y / cellSize)),
                 static_cast<int>(std::floor(p.z / cellSize)) };
    };

    // Build spatial hash from OBJ positions
    std::unordered_map<CellKey, std::vector<size_t>, CellHash> grid;
    for (size_t j = 0; j < objVertCount; ++j) {
        grid[toCell(objPositions[j])].push_back(j);
    }

    // Helper to read joint/weight data from a glTF vertex
    auto readGltfBoneData = [&](const GltfVertData& gv,
                                 glm::vec4& outJoints, glm::vec4& outWeights) {
        const auto& prim = primitives[gv.primitiveIdx];
        size_t i = gv.vertexIdx;

        if (prim.jointComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const uint16_t* jointData16 = reinterpret_cast<const uint16_t*>(prim.jointData);
            outJoints = glm::vec4(
                static_cast<float>(jointData16[i * 4 + 0]),
                static_cast<float>(jointData16[i * 4 + 1]),
                static_cast<float>(jointData16[i * 4 + 2]),
                static_cast<float>(jointData16[i * 4 + 3]));
        } else {
            outJoints = glm::vec4(
                static_cast<float>(prim.jointData[i * 4 + 0]),
                static_cast<float>(prim.jointData[i * 4 + 1]),
                static_cast<float>(prim.jointData[i * 4 + 2]),
                static_cast<float>(prim.jointData[i * 4 + 3]));
        }

        outWeights = glm::vec4(
            prim.weightData[i * 4 + 0],
            prim.weightData[i * 4 + 1],
            prim.weightData[i * 4 + 2],
            prim.weightData[i * 4 + 3]);
    };

    // Match each aligned glTF vertex to closest OBJ vertex within epsilon
    std::vector<bool> objMatched(objVertCount, false);
    uint32_t matchedCount = 0;
    for (size_t gi = 0; gi < gltfVerts.size(); ++gi) {
        CellKey cell = toCell(alignedGltfPos[gi]);

        float bestDist = epsilon;
        size_t bestIdx = SIZE_MAX;

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    CellKey neighbor = { cell.x + dx, cell.y + dy, cell.z + dz };
                    auto it = grid.find(neighbor);
                    if (it == grid.end()) continue;

                    for (size_t j : it->second) {
                        float d = glm::distance(alignedGltfPos[gi], objPositions[j]);
                        if (d < bestDist) {
                            bestDist = d;
                            bestIdx = j;
                        }
                    }
                }
            }
        }

        if (bestIdx != SIZE_MAX) {
            glm::vec4 joints, weights;
            readGltfBoneData(gltfVerts[gi], joints, weights);
            jointIndices[bestIdx] = joints;
            jointWeights[bestIdx] = weights;
            if (!objMatched[bestIdx]) {
                objMatched[bestIdx] = true;
                matchedCount++;
            }
        }
    }

    // Fallback: for unmatched OBJ vertices, brute-force find nearest aligned glTF vertex
    uint32_t unmatchedCount = static_cast<uint32_t>(objVertCount) - matchedCount;
    if (unmatchedCount > 0 && unmatchedCount < objVertCount) {
        uint32_t fallbackMatched = 0;
        for (size_t j = 0; j < objVertCount; ++j) {
            if (objMatched[j]) continue;

            float bestDist = std::numeric_limits<float>::max();
            size_t bestGi = SIZE_MAX;
            for (size_t gi = 0; gi < gltfVerts.size(); ++gi) {
                float d = glm::distance(objPositions[j], alignedGltfPos[gi]);
                if (d < bestDist) { bestDist = d; bestGi = gi; }
            }

            if (bestGi != SIZE_MAX) {
                glm::vec4 joints, weights;
                readGltfBoneData(gltfVerts[bestGi], joints, weights);
                jointIndices[j] = joints;
                jointWeights[j] = weights;
                fallbackMatched++;
            }
        }
        std::cout << "  Bone data fallback: " << fallbackMatched << " / "
                  << unmatchedCount << " remaining vertices" << std::endl;
        matchedCount += fallbackMatched;
    }

    std::cout << "  Bone data matched: " << matchedCount << " / " << objVertCount
              << " vertices" << std::endl;
}
