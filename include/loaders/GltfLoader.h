#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace tinygltf { class Model; }

// ============================================================================
// Data Structures
// ============================================================================

struct Skeleton {
    struct Bone {
        int nodeIndex;
        std::string name;
        int parentIndex = -1;
        std::vector<int> childrenIndices;
        glm::mat4 inverseBindMatrix = glm::mat4(1.0f);
        glm::mat4 localTransform = glm::mat4(1.0f);
        glm::vec3 animTranslation = glm::vec3(0.0f);
        glm::quat animRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 animScale = glm::vec3(1.0f);
    };
    std::vector<Bone> bones;
    glm::mat4 armatureTransform = glm::mat4(1.0f);  // Skeleton root node transform
    glm::mat4 objAlignTransform = glm::mat4(1.0f);  // glTF mesh-local → OBJ space
    glm::mat4 objAlignInverse = glm::mat4(1.0f);    // OBJ → glTF mesh-local space
};

struct KeyFrame {
    float time;
    glm::vec3 translation = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
};

struct AnimationChannel {
    int boneIndex;
    std::string path;           // "translation", "rotation", "scale"
    std::string interpolation;  // "LINEAR", "STEP"
    std::vector<KeyFrame> keyframes;
};

struct Animation {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationChannel> channels;
};

// ============================================================================
// Functions
// ============================================================================

class GltfLoader {
public:
    // Load a .gltf file and return the tinygltf model
    static tinygltf::Model loadModel(const std::string& filepath);

    // Extract skeleton hierarchy from the first skin
    static void extractSkeleton(const tinygltf::Model& model, Skeleton& skeleton);

    // Extract all animations and map channels to bone indices
    static void extractAnimations(const tinygltf::Model& model,
                                  const Skeleton& skeleton,
                                  std::vector<Animation>& animations);

    // Interpolate keyframes at the given time and update bone local transforms
    static void updateSkeleton(const Animation& animation, float time,
                               Skeleton& skeleton);

    // Compute final bone matrices: globalTransform * inverseBindMatrix
    static void computeBoneMatrices(const Skeleton& skeleton,
                                    std::vector<glm::mat4>& boneMatrices);

    // Match glTF vertex positions to OBJ vertex positions (O(n^2))
    // and transfer joint indices + weights
    static void matchBoneDataToObjMesh(const tinygltf::Model& model,
                                       const std::vector<glm::vec3>& objPositions,
                                       Skeleton& skeleton,
                                       std::vector<glm::vec4>& jointIndices,
                                       std::vector<glm::vec4>& jointWeights);
};
