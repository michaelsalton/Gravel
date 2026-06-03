#ifndef LODS_GLSL                                // Include guard: skip if already included this compilation unit
#define LODS_GLSL

#include "common.glsl"                           // alignRotationToVector and the offsetVertex placement convention
#include "../parametric/parametricSurfaces.glsl" // evaluateParametricSurface, to sample an element's local shape

// =============================================================================
// lods.glsl — Screen-space sizing and adaptive LOD selection
//
// Estimates how large a procedural element is on screen and maps that to a
// tessellation resolution, so distant elements use fewer triangles. It builds
// a local-space AABB by sampling the parametric surface, projects the AABB's 8
// corners to NDC to measure screen extent, then converts that to an M×N grid
// resolution. The task shader calls getScreenSpaceSize (coverage fade / proxy)
// and getLodMN (adaptive resolution).
// =============================================================================

// *** Michael Salton ***

// ============================================================================
// Bounding Box Computation
// ============================================================================

// Compute AABB for a parametric surface in local space.
// Samples 9 UV points (4 corners + 4 edge midpoints + center).
void parametricBoundingBox(vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling, // Local AABB of the element shape
                           uint elementType, float torusMajorR, float torusMinorR, float sphereRadius,
                           out vec3 minLocal, out vec3 maxLocal) {
    minLocal = vec3( 1e10);                      // Start min at +large
    maxLocal = vec3(-1e10);                      // Start max at -large

    // 9-point sample grid: corners, edge midpoints, center
    const vec2 sampleUV[9] = vec2[](             // Sample these 9 UVs to bound the surface cheaply
        vec2(0.0, 0.0),
        vec2(0.5, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 0.5),
        vec2(0.5, 0.5),
        vec2(1.0, 0.5),
        vec2(0.0, 1.0),
        vec2(0.5, 1.0),
        vec2(1.0, 1.0)
    );

    for (int i = 0; i < 9; i++) {                // For each sample UV...
        vec3 localPos, localNormal;              // Surface point/normal in local space
        evaluateParametricSurface(sampleUV[i], localPos, localNormal, elementType, // ...evaluate the element's shape
                                  torusMajorR, torusMinorR, sphereRadius, 0u, 0.0);
        minLocal = min(minLocal, localPos);      // Expand the AABB minimum
        maxLocal = max(maxLocal, localPos);      // Expand the AABB maximum
    }
}

// ============================================================================
// Screen-Space Size
// ============================================================================

// Projects all 8 AABB corners to NDC and returns the largest screen-space extent.
// Returns 10.0 if any corner is behind the camera (conservative: don't cull).
float computeScreenSpaceSize(vec3 minLocal, vec3 maxLocal,              // Largest NDC extent of the element's projected AABB
                             vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
                             mat4 mvp) {
    float scale    = sqrt(faceArea) * userScaling; // Element size (matches offsetVertex's √area rule)
    mat3  rotation = alignRotationToVector(elementNormal); // Element orientation frame

    vec2 screenMin = vec2( 1e10);                // NDC bounds accumulators
    vec2 screenMax = vec2(-1e10);

    // All 8 corners of the local AABB
    vec3 corners[8];                             // Enumerate the 8 AABB corners
    corners[0] = vec3(minLocal.x, minLocal.y, minLocal.z);
    corners[1] = vec3(maxLocal.x, minLocal.y, minLocal.z);
    corners[2] = vec3(minLocal.x, maxLocal.y, minLocal.z);
    corners[3] = vec3(maxLocal.x, maxLocal.y, minLocal.z);
    corners[4] = vec3(minLocal.x, minLocal.y, maxLocal.z);
    corners[5] = vec3(maxLocal.x, minLocal.y, maxLocal.z);
    corners[6] = vec3(minLocal.x, maxLocal.y, maxLocal.z);
    corners[7] = vec3(maxLocal.x, maxLocal.y, maxLocal.z);

    for (int i = 0; i < 8; i++) {                // Project each corner to NDC
        // scale → rotate → translate  (matches offsetVertex in common.glsl)
        vec3 worldPos = elementPos + rotation * (corners[i] * scale); // Place the corner in world space

        vec4 clipPos = mvp * vec4(worldPos, 1.0); // Project to clip space

        if (clipPos.w <= 0.0) {                  // Corner behind the camera...
            return 10.0;  // behind camera — treat as very large to avoid skipping // ...return "large" so it isn't culled
        }

        vec2 ndc = clipPos.xy / clipPos.w;       // Perspective divide → NDC
        screenMin = min(screenMin, ndc);         // Track NDC bounds
        screenMax = max(screenMax, ndc);
    }

    vec2 extent = screenMax - screenMin;         // NDC size of the projected box
    return max(extent.x, extent.y);              // Use the larger axis as the screen size
}

// ============================================================================
// Combined Helper
// ============================================================================

// Compute screen-space size directly from element data (bounding box + projection).
float getScreenSpaceSize(vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling, // One-shot AABB → screen size
                         uint elementType, float torusMajorR, float torusMinorR, float sphereRadius,
                         mat4 mvp) {
    vec3 minLocal, maxLocal;                     // Local AABB
    parametricBoundingBox(elementPos, elementNormal, faceArea, userScaling, // Build it from the element shape
                          elementType, torusMajorR, torusMinorR, sphereRadius,
                          minLocal, maxLocal);

    return computeScreenSpaceSize(minLocal, maxLocal, elementPos, elementNormal, // Project it and return the screen extent
                                  faceArea, userScaling, mvp);
}

// ============================================================================
// LOD Resolution Selection
// ============================================================================

// Maps screen-space size to a UV grid resolution.
// Formula: resolution = baseMN * sqrt(screenSize * lodFactor)
// sqrt() ensures resolution scales as the square root of screen area,
// keeping pixel density roughly constant.
uint computeLodResolution(float screenSize, uint baseMN, float lodFactor, // Screen size → tessellation resolution
                          uint minResolution, uint maxResolution) {
    float target = float(baseMN) * sqrt(screenSize * lodFactor); // Scale base resolution by √(size × quality)
    uint resolution = uint(target);             // Truncate to an integer resolution
    resolution = max(resolution, minResolution); // Clamp to the minimum
    resolution = min(resolution, maxResolution); // Clamp to the maximum
    return resolution;
}

// Compute adaptive M×N resolution from element world-space data.
void getLodMN(vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling, // Pick adaptive M,N for an element
              uint elementType, float torusMajorR, float torusMinorR, float sphereRadius,
              mat4 mvp, uint baseMN, float lodFactor, uint minResolution, uint maxResolution,
              out uint outM, out uint outN) {
    float screenSize = getScreenSpaceSize(elementPos, elementNormal, faceArea, userScaling, // Estimate on-screen size
                                          elementType, torusMajorR, torusMinorR, sphereRadius, mvp);

    uint resolution = computeLodResolution(screenSize, baseMN, lodFactor, minResolution, maxResolution); // Map to resolution
    outM = resolution;                           // Use a square grid: M = N = resolution
    outN = resolution;
}

#endif // LODS_GLSL

// *** ************ ***
