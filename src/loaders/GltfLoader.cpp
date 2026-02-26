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

    // Compute inverse of the skinned mesh node's global transform.
    // glTF spec: jointMatrix = inverse(meshNodeGlobal) * jointGlobal * IBM
    // Find the node that references skin 0, then walk up the scene graph.
    {
        // Build parent map for all nodes
        std::unordered_map<int, int> parentMap;
        for (size_t p = 0; p < model.nodes.size(); ++p) {
            for (int child : model.nodes[p].children) {
                parentMap[child] = static_cast<int>(p);
            }
        }

        // Find the node with skin == 0
        int skinNodeIdx = -1;
        for (size_t n = 0; n < model.nodes.size(); ++n) {
            if (model.nodes[n].skin == 0) {
                skinNodeIdx = static_cast<int>(n);
                break;
            }
        }

        if (skinNodeIdx >= 0) {
            // Compute global transform by walking up to the root
            glm::mat4 globalT = getNodeTransform(model.nodes[skinNodeIdx]);
            int current = skinNodeIdx;
            while (parentMap.count(current)) {
                current = parentMap[current];
                globalT = getNodeTransform(model.nodes[current]) * globalT;
            }
            skeleton.skinNodeGlobalInverse = glm::inverse(globalT);
            std::cout << "  Skin node: " << model.nodes[skinNodeIdx].name
                      << " (global transform has scale/rotation from armature)" << std::endl;
        }
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

            // Convert strings to enums (one-time at load)
            if (channel.target_path == "translation") animChannel.path = AnimPath::Translation;
            else if (channel.target_path == "rotation") animChannel.path = AnimPath::Rotation;
            else if (channel.target_path == "scale") animChannel.path = AnimPath::Scale;
            else continue; // Unsupported path

            animChannel.interpolation = (interpolation == "STEP")
                ? AnimInterp::Step : AnimInterp::Linear;

            // Determine value components per keyframe
            size_t valueComponents = (animChannel.path == AnimPath::Rotation) ? 4 : 3;

            // Create keyframes
            for (size_t i = 0; i < times.size(); ++i) {
                KeyFrame keyframe;
                keyframe.time = times[i];
                if (keyframe.time > animation.duration) {
                    animation.duration = keyframe.time;
                }

                size_t offset = i * valueComponents;

                if (animChannel.path == AnimPath::Translation) {
                    keyframe.translation = glm::make_vec3(&values[offset]);
                } else if (animChannel.path == AnimPath::Rotation) {
                    keyframe.rotation = glm::make_quat(&values[offset]);
                } else if (animChannel.path == AnimPath::Scale) {
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
        if (channel.interpolation == AnimInterp::Linear) {
            if (channel.path == AnimPath::Translation) {
                bone.animTranslation = glm::mix(kf0.translation, kf1.translation, t);
            } else if (channel.path == AnimPath::Rotation) {
                bone.animRotation = glm::slerp(kf0.rotation, kf1.rotation, t);
            } else if (channel.path == AnimPath::Scale) {
                bone.animScale = glm::mix(kf0.scale, kf1.scale, t);
            }
        } else if (channel.interpolation == AnimInterp::Step) {
            if (channel.path == AnimPath::Translation) {
                bone.animTranslation = kf0.translation;
            } else if (channel.path == AnimPath::Rotation) {
                bone.animRotation = kf0.rotation;
            } else if (channel.path == AnimPath::Scale) {
                bone.animScale = kf0.scale;
            }
        }

        // Reconstruct local transform from TRS
        bone.localTransform = glm::translate(glm::mat4(1.0f), bone.animTranslation) *
                              glm::mat4_cast(bone.animRotation) *
                              glm::scale(glm::mat4(1.0f), bone.animScale);
    }
}

// ============================================================================
// Spatial matching helpers (shared by matchBoneDataToObjMesh & matchUVsToObjMesh)
// ============================================================================

// Compute uniform scale and offset to align gltfPositions onto objPositions via AABB matching
static void computeAlignment(const std::vector<glm::vec3>& objPositions,
                              const std::vector<glm::vec3>& gltfPositions,
                              float& outScale, glm::vec3& outOffset) {
    glm::vec3 objMin(std::numeric_limits<float>::max());
    glm::vec3 objMax(std::numeric_limits<float>::lowest());
    for (const auto& p : objPositions) {
        objMin = glm::min(objMin, p);
        objMax = glm::max(objMax, p);
    }
    glm::vec3 gltfMin(std::numeric_limits<float>::max());
    glm::vec3 gltfMax(std::numeric_limits<float>::lowest());
    for (const auto& p : gltfPositions) {
        gltfMin = glm::min(gltfMin, p);
        gltfMax = glm::max(gltfMax, p);
    }

    glm::vec3 objExtent = objMax - objMin;
    glm::vec3 gltfExtent = gltfMax - gltfMin;
    glm::vec3 objCenter = (objMin + objMax) * 0.5f;
    glm::vec3 gltfCenter = (gltfMin + gltfMax) * 0.5f;

    int bestAxis = 0;
    for (int a = 1; a < 3; ++a) {
        if (gltfExtent[a] > gltfExtent[bestAxis]) bestAxis = a;
    }
    outScale = 1.0f;
    if (gltfExtent[bestAxis] > 1e-6f) {
        outScale = objExtent[bestAxis] / gltfExtent[bestAxis];
    }
    outOffset = objCenter - outScale * gltfCenter;
}

// For each OBJ vertex, find the index of the closest aligned glTF vertex (or SIZE_MAX).
// alignedGltfPos must already be transformed to OBJ coordinate space.
static std::vector<size_t> spatialMatch(
    const std::vector<glm::vec3>& objPositions,
    const std::vector<glm::vec3>& alignedGltfPos) {

    size_t objVertCount = objPositions.size();
    size_t gltfVertCount = alignedGltfPos.size();

    // Compute OBJ bounding box for epsilon scaling
    glm::vec3 objMin(std::numeric_limits<float>::max());
    glm::vec3 objMax(std::numeric_limits<float>::lowest());
    for (const auto& p : objPositions) {
        objMin = glm::min(objMin, p);
        objMax = glm::max(objMax, p);
    }
    glm::vec3 objExtent = objMax - objMin;
    int bestAxis = 0;
    for (int a = 1; a < 3; ++a) {
        if (objExtent[a] > objExtent[bestAxis]) bestAxis = a;
    }

    // Adaptive epsilon from sampled nearest-neighbor distances
    float epsilon = 1e-5f;
    {
        size_t sampleCount = std::min(size_t(200), gltfVertCount);
        size_t step = std::max(size_t(1), gltfVertCount / sampleCount);
        std::vector<float> nnDists;
        nnDists.reserve(sampleCount);

        for (size_t s = 0; s < gltfVertCount && nnDists.size() < sampleCount; s += step) {
            float bestDist = std::numeric_limits<float>::max();
            for (size_t j = 0; j < objVertCount; ++j) {
                float d = glm::distance(alignedGltfPos[s], objPositions[j]);
                if (d < bestDist) bestDist = d;
            }
            float maxReasonable = objExtent[bestAxis] * 0.1f;
            if (bestDist < maxReasonable) {
                nnDists.push_back(bestDist);
            }
        }

        if (!nnDists.empty()) {
            std::sort(nnDists.begin(), nnDists.end());
            float median = nnDists[nnDists.size() / 2];
            epsilon = std::clamp(median * 3.0f, 1e-5f, objExtent[bestAxis] * 0.02f);
        }
    }

    float cellSize = std::max(epsilon * 3.0f, 1e-4f);

    // Spatial hash
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

    // For each OBJ vertex, track the closest glTF match
    std::vector<size_t> objToGltf(objVertCount, SIZE_MAX);
    std::vector<float> objBestDist(objVertCount, std::numeric_limits<float>::max());

    // Match each glTF vertex to closest OBJ vertex within epsilon
    for (size_t gi = 0; gi < gltfVertCount; ++gi) {
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

        if (bestIdx != SIZE_MAX && bestDist < objBestDist[bestIdx]) {
            objBestDist[bestIdx] = bestDist;
            objToGltf[bestIdx] = gi;
        }
    }

    // Fallback: brute-force for unmatched OBJ vertices
    uint32_t matchedCount = 0;
    for (size_t j = 0; j < objVertCount; ++j) {
        if (objToGltf[j] != SIZE_MAX) { matchedCount++; continue; }

        float bestDist = std::numeric_limits<float>::max();
        size_t bestGi = SIZE_MAX;
        for (size_t gi = 0; gi < gltfVertCount; ++gi) {
            float d = glm::distance(objPositions[j], alignedGltfPos[gi]);
            if (d < bestDist) { bestDist = d; bestGi = gi; }
        }
        if (bestGi != SIZE_MAX) {
            objToGltf[j] = bestGi;
        }
    }

    return objToGltf;
}

void GltfLoader::matchBoneDataToObjMesh(const tinygltf::Model& model,
                                         const std::vector<glm::vec3>& objPositions,
                                         Skeleton& skeleton,
                                         std::vector<glm::vec4>& jointIndices,
                                         std::vector<glm::vec4>& jointWeights) {
    size_t objVertCount = objPositions.size();
    jointIndices.assign(objVertCount, glm::vec4(0.0f));
    jointWeights.assign(objVertCount, glm::vec4(0.0f));

    // Collect all glTF vertex positions and per-primitive bone data pointers
    struct PrimitiveData {
        const uint8_t* jointData;
        const float* weightData;
        int jointComponentType;
    };
    std::vector<PrimitiveData> primitives;
    std::vector<glm::vec3> gltfPositions;
    std::vector<size_t> gltfPrimIdx;   // per-vertex: which primitive
    std::vector<size_t> gltfVertIdx;   // per-vertex: index within primitive

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

            size_t count = posAccessor.count;
            for (size_t i = 0; i < count; ++i) {
                gltfPositions.push_back(glm::vec3(posData[i * 3 + 0], posData[i * 3 + 1], posData[i * 3 + 2]));
                gltfPrimIdx.push_back(primIdx);
                gltfVertIdx.push_back(i);
            }
        }
    }

    if (gltfPositions.empty()) {
        std::cerr << "  No glTF vertices found for bone matching." << std::endl;
        return;
    }

    // Compute alignment and store in skeleton for bone matrix conjugation
    float scale;
    glm::vec3 offset;
    computeAlignment(objPositions, gltfPositions, scale, offset);

    skeleton.objAlignTransform = glm::translate(glm::mat4(1.0f), offset) *
                                  glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    skeleton.objAlignInverse = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / scale)) *
                                glm::translate(glm::mat4(1.0f), -offset);

    std::cout << "  Bone matching: scale=" << scale << std::endl;

    // Transform glTF positions to OBJ space
    std::vector<glm::vec3> alignedGltfPos(gltfPositions.size());
    for (size_t i = 0; i < gltfPositions.size(); ++i) {
        alignedGltfPos[i] = scale * gltfPositions[i] + offset;
    }

    // Spatial match: for each OBJ vertex, find the closest glTF vertex
    std::vector<size_t> objToGltf = spatialMatch(objPositions, alignedGltfPos);

    // Helper to read joint/weight data from a glTF vertex
    auto readGltfBoneData = [&](size_t gi, glm::vec4& outJoints, glm::vec4& outWeights) {
        const auto& prim = primitives[gltfPrimIdx[gi]];
        size_t i = gltfVertIdx[gi];

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

    // Transfer bone data using the spatial match results
    uint32_t matchedCount = 0;
    for (size_t j = 0; j < objVertCount; ++j) {
        if (objToGltf[j] == SIZE_MAX) continue;
        glm::vec4 joints, weights;
        readGltfBoneData(objToGltf[j], joints, weights);
        jointIndices[j] = joints;
        jointWeights[j] = weights;
        matchedCount++;
    }

    std::cout << "  Bone data matched: " << matchedCount << " / " << objVertCount
              << " vertices" << std::endl;
}

void GltfLoader::matchUVsToObjMesh(const tinygltf::Model& model,
                                    const std::vector<glm::vec3>& objPositions,
                                    const Skeleton& skeleton,
                                    std::vector<glm::vec2>& outUVs) {
    size_t objVertCount = objPositions.size();
    outUVs.assign(objVertCount, glm::vec2(0.0f));

    // Collect glTF vertex positions and UVs
    std::vector<glm::vec3> gltfPositions;
    std::vector<glm::vec2> gltfUVs;

    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            auto posIt = primitive.attributes.find("POSITION");
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            if (posIt == primitive.attributes.end() || uvIt == primitive.attributes.end())
                continue;

            const tinygltf::Accessor& posAcc = model.accessors[posIt->second];
            const tinygltf::BufferView& posBV = model.bufferViews[posAcc.bufferView];
            const float* posData = reinterpret_cast<const float*>(
                &model.buffers[posBV.buffer].data[posAcc.byteOffset + posBV.byteOffset]);

            const tinygltf::Accessor& uvAcc = model.accessors[uvIt->second];
            const tinygltf::BufferView& uvBV = model.bufferViews[uvAcc.bufferView];
            const float* uvData = reinterpret_cast<const float*>(
                &model.buffers[uvBV.buffer].data[uvAcc.byteOffset + uvBV.byteOffset]);

            size_t count = posAcc.count;
            for (size_t i = 0; i < count; ++i) {
                gltfPositions.push_back(glm::vec3(posData[i * 3 + 0], posData[i * 3 + 1], posData[i * 3 + 2]));
                gltfUVs.push_back(glm::vec2(uvData[i * 2 + 0], uvData[i * 2 + 1]));
            }
        }
    }

    if (gltfPositions.empty()) {
        std::cerr << "  No TEXCOORD_0 found in glTF mesh." << std::endl;
        return;
    }

    std::cout << "  UV matching: " << gltfPositions.size() << " glTF verts -> "
              << objVertCount << " OBJ verts" << std::endl;

    // Compute alignment: reuse skeleton's if available, otherwise compute from AABBs
    float scale;
    glm::vec3 offset;
    if (skeleton.objAlignTransform != glm::mat4(1.0f)) {
        scale = skeleton.objAlignTransform[0][0];
        offset = glm::vec3(skeleton.objAlignTransform[3]);
    } else {
        computeAlignment(objPositions, gltfPositions, scale, offset);
    }

    // Transform glTF positions to OBJ space
    std::vector<glm::vec3> alignedPos(gltfPositions.size());
    for (size_t i = 0; i < gltfPositions.size(); ++i) {
        alignedPos[i] = scale * gltfPositions[i] + offset;
    }

    // Spatial match: for each OBJ vertex, find the closest glTF vertex
    std::vector<size_t> objToGltf = spatialMatch(objPositions, alignedPos);

    // Transfer UV data using the spatial match results
    uint32_t matchedCount = 0;
    for (size_t j = 0; j < objVertCount; ++j) {
        if (objToGltf[j] == SIZE_MAX) continue;
        outUVs[j] = gltfUVs[objToGltf[j]];
        matchedCount++;
    }

    std::cout << "  UV data matched: " << matchedCount << " / " << objVertCount
              << " vertices" << std::endl;
}
