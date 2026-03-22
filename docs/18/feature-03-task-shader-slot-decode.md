# Feature 18.3: Task Shader Slot Decoding and Position Interpolation

**Epic**: [Epic 18 - GRWM Slot Placement](epic-18-grwm-slot-placement.md)

## Goal

Modify the task shader to decode the compound index into (faceId, slotIdx), read the slot's (u, v) from the SSBO, and interpolate the element position and normal from the face's vertex data.

## Files Modified

- `shaders/parametric/parametric.task`

## Existing Functions Used

- `getSlotEntry(faceId, slotIdx, slotsPerFace)` — already in `shaderInterface.h`, reads from SSBO binding 6
- `readFaceEdge(faceId)` — gets first half-edge of face
- `getHalfEdgeNext(he)` — traverses face loop
- `getHalfEdgeVertex(he)` — gets vertex at half-edge
- `readVertexPosition(vertId)` — vertex position
- `readVertexNormal(vertId)` — vertex normal (for smooth interpolation)

## Changes

### 1. Decode Slot Index (after globalId resolution, ~line 125)

```glsl
uint slotIdx = 0u;
bool useSlots = (push.activeSlots > 0u);

if (useSlots) {
    slotIdx = globalId % push.activeSlots;
    globalId = globalId / push.activeSlots;
}

// Vertex elements only when slots are off
bool isVertex = (!useSlots) && (globalId >= push.nbFaces);
```

### 2. Interpolate Position from Slot UV (in face branch, after existing position setup)

When `useSlots` is true, override the face-center position with the slot's interpolated position:

```glsl
if (useSlots) {
    SlotEntry slot = getSlotEntry(faceId, slotIdx, resurfacingUBO.preprocessSlotsPerFace);
    float su = slot.u;
    float sv = slot.v;

    // Gather face vertices via half-edge traversal
    int he0 = readFaceEdge(faceId);
    int he1 = getHalfEdgeNext(uint(he0));
    int he2 = getHalfEdgeNext(uint(he1));

    vec3 v0 = readVertexPosition(uint(getHalfEdgeVertex(uint(he0))));
    vec3 v1 = readVertexPosition(uint(getHalfEdgeVertex(uint(he1))));
    vec3 v2 = readVertexPosition(uint(getHalfEdgeVertex(uint(he2))));

    vec3 n0 = readVertexNormal(uint(getHalfEdgeVertex(uint(he0))));
    vec3 n1 = readVertexNormal(uint(getHalfEdgeVertex(uint(he1))));
    vec3 n2 = readVertexNormal(uint(getHalfEdgeVertex(uint(he2))));

    // Check if quad (4th half-edge loops back to he0)
    int he3 = getHalfEdgeNext(uint(he2));
    bool isQuad = (he3 != he0);

    if (isQuad) {
        // Bilinear interpolation: u,v in [0,1]^2
        vec3 v3 = readVertexPosition(uint(getHalfEdgeVertex(uint(he3))));
        vec3 n3 = readVertexNormal(uint(getHalfEdgeVertex(uint(he3))));
        payload.position = (1.0-su)*(1.0-sv)*v0 + su*(1.0-sv)*v1
                         + su*sv*v2 + (1.0-su)*sv*v3;
        payload.normal = normalize(
            (1.0-su)*(1.0-sv)*n0 + su*(1.0-sv)*n1
          + su*sv*n2 + (1.0-su)*sv*n3);
    } else {
        // Barycentric interpolation for triangles
        payload.position = (1.0-su-sv)*v0 + su*v1 + sv*v2;
        payload.normal = normalize((1.0-su-sv)*n0 + su*n1 + sv*n2);
    }

    // Each slot element occupies 1/K of the face area
    payload.area = readFaceArea(faceId) / float(push.activeSlots);
}
```

### 3. Unique Task ID

For per-element hash variation (color jitter, type selection):

```glsl
payload.taskId = useSlots ? (faceId * push.activeSlots + slotIdx) : globalId;
```

## Quad Face Caveat

GRWM triangulates meshes before computing slots, so slot (u,v) are in triangle barycentric space. When Gravel's mesh has quads, the remapped slots (merged from child triangles) may have (u,v) that don't map correctly to the quad's bilinear space.

**Mitigation**: Auto-set `triangulateMesh = true` when slot placement is enabled, ensuring the mesh topology matches GRWM's triangle-based slot coordinates. This can be relaxed later by converting coordinates during the CPU remap step.

## Verification

1. **K=1**: One element near face center (highest-priority slot). Should look similar to no-slot mode but position may differ slightly from exact center.
2. **K=8**: 8 elements per face scattered at priority positions. Visible density increase.
3. **K=64**: Maximum density. All slot positions populated.
4. **Toggle off**: Returns to single-element-per-face mode instantly.
5. **LOD + slots**: Distant faces should have smaller slot elements (area scaled by 1/K then further by LOD).
6. **Curvature + slots**: Curvature scaling should still apply on top of slot placement.
