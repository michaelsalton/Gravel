# Feature 10.4: glTF Loader & Skeleton Data

**Epic**: [Epic 10 - Textures, Skeleton & Bone Skinning](epic-10-textures-skeleton-skinning.md)
**Prerequisites**: [Feature 10.1](feature-01-image-loading-texture-infra.md) (for stb_image, shared by tinygltf)

## Goal

Add tinygltf library and create a glTF loader that extracts skeleton hierarchy, animation keyframes, and per-vertex skinning weights. Match glTF vertices to OBJ vertices by position comparison to transfer joint data. This phase is CPU-only — no GPU upload or skinning yet.

## What You'll Build

- tinygltf integration for parsing .gltf + .bin files
- Data structures: `Skeleton`, `Bone`, `Animation`, `AnimationChannel`, `KeyFrame`
- Skeleton extraction (joints, inverse bind matrices, parent hierarchy)
- Animation extraction (keyframes with LINEAR/STEP interpolation)
- Skeleton update (keyframe interpolation with mix + slerp)
- Bone matrix computation (recursive global transform × inverse bind matrix)
- Vertex matching (O(n²) position comparison to transfer joint indices/weights from glTF to OBJ mesh)

## Dragon glTF Data

| Model | Bones | Animations | Joints/Vertex |
|-------|-------|------------|---------------|
| dragon_8k.gltf | 7 (Bone - Bone.006) | 1 ("metarig", 21 channels) | 4 (JOINTS_0 + WEIGHTS_0) |
| dragon_coat.gltf | 7 (same skeleton) | 1 (same) | 4 |

## Files to Create

- `libs/tinygltf/tiny_gltf.h` — Copy from reference project
- `libs/tinygltf/json.hpp` — Copy from reference (nlohmann JSON, bundled)
- `libs/tinygltf/stb_image.h` — Copy from reference's tinygltf dir (tinygltf expects its own)
- `libs/tinygltf/stb_image_write.h` — Copy from reference's tinygltf dir
- `include/loaders/GltfLoader.h` — Data structures and function declarations
- `src/loaders/GltfLoader.cpp` — tinygltf implementation + all ported functions

## Files to Modify

- `CMakeLists.txt` — Add `src/loaders/GltfLoader.cpp` to SOURCES, `libs/tinygltf` to include dirs

## Implementation Steps

### Step 1: Copy tinygltf headers

Copy from `D:\Development\Research\resurfacing\libs\tinygltf\`:
- `tiny_gltf.h`
- `json.hpp`
- `stb_image.h` (tinygltf's bundled copy)
- `stb_image_write.h` (tinygltf's bundled copy)

### Step 2: Define data structures

```cpp
// include/loaders/GltfLoader.h
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
};

struct KeyFrame {
    float time;
    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;
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
```

### Step 3: Implement GltfLoader

In `src/loaders/GltfLoader.cpp`:

```cpp
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE        // Already defined in ImageLoader.cpp
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>
```

### Step 4: Port extractSkeleton()

From reference `GLTFLoader.cpp:9-60`:
- Read `model.skins[0]` to get joint node indices
- Read inverse bind matrices from the accessor → buffer view → buffer chain
- Build bone hierarchy by searching for parent-child relationships among joint nodes
- Extract initial TRS (translation, rotation, scale) from each node

### Step 5: Port extractAnimations()

From reference `GLTFLoader.cpp:83-185`:
- For each animation channel, read input (times) and output (values) from accessors
- Map target_node to bone index
- Support LINEAR and STEP interpolation
- Compute animation duration as max keyframe time

### Step 6: Port updateSkeleton()

From reference `GLTFLoader.cpp:187-216`:
- For each channel, find surrounding keyframes at current time
- Interpolate: `glm::mix` for translation/scale, `glm::slerp` for rotation
- Reconstruct `bone.localTransform` from TRS: translate × rotate × scale
- Handle animation looping via `fmod(time, duration)`

### Step 7: Port computeBoneMatrices()

From reference `GLTFLoader.cpp:73-81`:
- Recursively compute global transforms: `parent_global × local_transform`
- Final bone matrix: `globalTransform × inverseBindMatrix`

### Step 8: Port matchBoneDataToObjMesh()

From reference `GLTFLoader.cpp:255-314`:
- Read POSITION, JOINTS_0, WEIGHTS_0 attributes from glTF mesh
- For each glTF vertex, find matching OBJ vertex by position (epsilon = 1e-5)
- Copy joint indices (4 × uint8 → vec4 float) and weights (4 × float → vec4) to output vectors
- Note: This is O(n²) — acceptable for load-time

### Step 9: Update CMakeLists.txt

Add `src/loaders/GltfLoader.cpp` to SOURCES. Add `${CMAKE_SOURCE_DIR}/libs/tinygltf` to include dirs.

## Acceptance Criteria

- [ ] tinygltf compiles without errors
- [ ] `extractSkeleton()` finds 7 bones for dragon_8k.gltf
- [ ] `extractAnimations()` finds 1 animation with 21 channels
- [ ] `matchBoneDataToObjMesh()` assigns joint data to all referenced OBJ vertices
- [ ] `computeBoneMatrices()` produces valid matrices (no NaN/inf)
- [ ] `updateSkeleton()` interpolates keyframes and updates local transforms
- [ ] Console output confirms bone count and animation duration on dragon load
- [ ] No build warnings from tinygltf integration
