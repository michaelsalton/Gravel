# Feature 18.2: CPU Pre-Cull Loop Expansion

**Epic**: [Epic 18 - GRWM Slot Placement](epic-18-grwm-slot-placement.md)

## Goal

When slot placement is active, expand the CPU pre-cull loop to emit K indices per visible face instead of 1. Each index encodes both the face ID and slot index.

## Files Modified

- `src/renderer/renderer.cpp`
- `include/renderer/renderer.h`

## Encoding Scheme

When `activeSlots > 0`, the visible indices buffer contains compound indices:
- `faceId * K + slotIdx` for each face's K active slots
- Task shader decodes: `faceId = index / K`, `slotIdx = index % K`

When `activeSlots == 0`, existing behavior is unchanged (raw face/vertex indices).

## Changes

### 1. Increase VISIBLE_INDICES_MAX (renderer.h)

Change from 65536 to 262144:
```cpp
static constexpr uint32_t VISIBLE_INDICES_MAX = 262144;
```

Also increase the GPU buffer allocation in `renderer_init.cpp` to match.

### 2. Expand Pre-Cull Loop (renderer.cpp, ~line 628)

When `activeSlots > 0`:

```cpp
uint32_t K = pushConstants.activeSlots;
for (uint32_t i = 0; i < heNbFaces; i++) {
    if (isMasked(cpuFaceUVs[i])) continue;
    cachedTotalElements += K;
    if (isVisible(cpuFaceCenters[i], cpuFaceNormals[i], cpuFaceAreas[i])) {
        for (uint32_t s = 0; s < K; s++) {
            if (cachedVisibleIndices.size() < VISIBLE_INDICES_MAX)
                cachedVisibleIndices.push_back(i * K + s);
        }
    }
}
// Skip vertex elements in slot mode
```

When `activeSlots == 0`: existing loop unchanged (faces + vertices).

### 3. Cache Invalidation (renderer.cpp, ~line 586)

Add `enableSlotPlacement` and `activeSlotCount` to the settings-changed detection so the visible cache rebuilds when the user toggles slot settings.

### 4. Element Count Stats

Update `cachedEstMeshShaders` and element count calculations to account for the K multiplier when slots are active.

## Verification

- With slots disabled: identical behavior to before
- With slots enabled (K=8): visible indices buffer contains 8x more entries per visible face
- Stats panel shows correct element counts
- No buffer overflow with large meshes (262144 limit)
