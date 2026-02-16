# Feature 7.6: Skeletal Animation (Optional)

**Epic**: [Epic 7 - Control Maps and Polish](../../07/epic-07-control-maps.md)
**Estimated Time**: 4-5 hours (complex, optional)
**Prerequisites**: [Feature 7.5 - Debug Modes](feature-05-debug-modes.md)

## Goal

Add skeletal animation support to animate base meshes with resurfacing that follows the deformation.

## Overview

This is an **optional advanced feature** that adds significant complexity. Skip if not needed.

## Implementation Steps

### Step 1: Add tinygltf Dependency

```cmake
# In CMakeLists.txt
add_subdirectory(libs/tinygltf)
target_link_libraries(Gravel PRIVATE tinygltf)
```

### Step 2: Load GLTF with Animation

```cpp
// src/loaders/GLTFLoader.hpp
class GLTFLoader {
public:
    struct AnimatedMesh {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec4> jointIndices;
        std::vector<glm::vec4> jointWeights;
        std::vector<glm::mat4> inverseBindMatrices;
        // ... animation data
    };

    static AnimatedMesh loadGLTF(const std::string& filepath);
};
```

### Step 3: Upload Skinning Data to GPU

```glsl
// Add to descriptor set bindings
layout(set = SET_PER_OBJECT, binding = 3) readonly buffer JointMatrices {
    mat4 joints[];
};

layout(set = SET_PER_OBJECT, binding = 4) readonly buffer JointIndices {
    uvec4 indices[];
};

layout(set = SET_PER_OBJECT, binding = 5) readonly buffer JointWeights {
    vec4 weights[];
};
```

### Step 4: Apply Skinning in Task Shader

```glsl
vec3 getSkinnedPosition(uint vertId) {
    vec3 pos = readVertexPosition(vertId);
    uvec4 jointIdx = indices[vertId];
    vec4 weights = weights[vertId];

    vec3 skinnedPos = vec3(0);
    for (int i = 0; i < 4; i++) {
        if (weights[i] > 0.0) {
            mat4 jointMat = joints[jointIdx[i]];
            skinnedPos += (jointMat * vec4(pos, 1.0)).xyz * weights[i];
        }
    }

    return skinnedPos;
}

void main() {
    // Use skinned positions instead of static positions
    vec3 position = getSkinnedPosition(vertId);
    vec3 normal = getSkinnedNormal(vertId);

    // ... rest of task shader
}
```

### Step 5: Animate Skeleton

```cpp
// In main loop
void updateAnimation(float deltaTime) {
    currentTime += deltaTime;

    // Sample animation at current time
    // Update joint matrices
    // Upload to GPU

    uploadJointMatrices(device, jointMatricesBuffer, jointMatrices);
}
```

## Acceptance Criteria

- [ ] GLTF loads with skeleton
- [ ] Animation plays smoothly
- [ ] Resurfacing follows deformation
- [ ] No breaks in skinning
- [ ] 60+ FPS with animation

## Complexity Warning

This feature is **significantly more complex** than others. Consider whether it's needed for your use case.

**Simpler alternative**: Use static meshes and manually create multiple poses.

## Summary

**Epic 7 Complete!**

You've implemented:
- ✅ Control map texture loading
- ✅ Control map sampling for hybrid surfaces
- ✅ Base mesh rendering (solid/wireframe)
- ✅ UI organization with presets
- ✅ Debug visualization modes
- ⚪ Skeletal animation (optional)

The system now has professional UI, control maps for artistic control, and comprehensive debugging tools!

## Next Epic

Next epic: **[Epic 8 - Performance Analysis](../../08/epic-08-performance-analysis.md)**

Epic 8 will focus on profiling, optimization, and final polish.
