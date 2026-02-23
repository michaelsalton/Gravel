#ifndef LODS_GLSL
#define LODS_GLSL

#include "common.glsl"
#include "parametric/parametricSurfaces.glsl"

// ============================================================================
// Bounding Box Computation
// ============================================================================

// LUT-based element types (B-Spline and Bézier use the pre-computed cage AABB)
#define LOD_ELEMENT_BSPLINE 4u
#define LOD_ELEMENT_BEZIER  5u

// Compute AABB for a parametric surface in local space.
// For LUT surfaces uses the pre-computed cage bounding box; for analytical
// surfaces samples 9 UV points (4 corners + 4 edge midpoints + center).
void parametricBoundingBox(vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
                           uint elementType, float torusMajorR, float torusMinorR, float sphereRadius,
                           out vec3 minLocal, out vec3 maxLocal) {
    // LUT surfaces: use exact bounding box stored in the ResurfacingUBO
    if (elementType == LOD_ELEMENT_BSPLINE || elementType == LOD_ELEMENT_BEZIER) {
        minLocal = resurfacingUBO.lutBBMin.xyz;
        maxLocal = resurfacingUBO.lutBBMax.xyz;
        return;
    }

    minLocal = vec3( 1e10);
    maxLocal = vec3(-1e10);

    // 9-point sample grid: corners, edge midpoints, center
    const vec2 sampleUV[9] = vec2[](
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

    for (int i = 0; i < 9; i++) {
        vec3 localPos, localNormal;
        evaluateParametricSurface(sampleUV[i], localPos, localNormal, elementType,
                                  torusMajorR, torusMinorR, sphereRadius);
        minLocal = min(minLocal, localPos);
        maxLocal = max(maxLocal, localPos);
    }
}

// ============================================================================
// Screen-Space Size
// ============================================================================

// Projects all 8 AABB corners to NDC and returns the largest screen-space extent.
// Returns 10.0 if any corner is behind the camera (conservative: don't cull).
float computeScreenSpaceSize(vec3 minLocal, vec3 maxLocal,
                             vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
                             mat4 mvp) {
    float scale    = sqrt(faceArea) * userScaling;
    mat3  rotation = alignRotationToVector(elementNormal);

    vec2 screenMin = vec2( 1e10);
    vec2 screenMax = vec2(-1e10);

    // All 8 corners of the local AABB
    vec3 corners[8];
    corners[0] = vec3(minLocal.x, minLocal.y, minLocal.z);
    corners[1] = vec3(maxLocal.x, minLocal.y, minLocal.z);
    corners[2] = vec3(minLocal.x, maxLocal.y, minLocal.z);
    corners[3] = vec3(maxLocal.x, maxLocal.y, minLocal.z);
    corners[4] = vec3(minLocal.x, minLocal.y, maxLocal.z);
    corners[5] = vec3(maxLocal.x, minLocal.y, maxLocal.z);
    corners[6] = vec3(minLocal.x, maxLocal.y, maxLocal.z);
    corners[7] = vec3(maxLocal.x, maxLocal.y, maxLocal.z);

    for (int i = 0; i < 8; i++) {
        // scale → rotate → translate  (matches offsetVertex in common.glsl)
        vec3 worldPos = elementPos + rotation * (corners[i] * scale);

        vec4 clipPos = mvp * vec4(worldPos, 1.0);

        if (clipPos.w <= 0.0) {
            return 10.0;  // behind camera — treat as very large to avoid skipping
        }

        vec2 ndc = clipPos.xy / clipPos.w;
        screenMin = min(screenMin, ndc);
        screenMax = max(screenMax, ndc);
    }

    vec2 extent = screenMax - screenMin;
    return max(extent.x, extent.y);
}

// ============================================================================
// Combined Helper
// ============================================================================

// Compute screen-space size directly from element data (bounding box + projection).
float getScreenSpaceSize(vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
                         uint elementType, float torusMajorR, float torusMinorR, float sphereRadius,
                         mat4 mvp) {
    vec3 minLocal, maxLocal;
    parametricBoundingBox(elementPos, elementNormal, faceArea, userScaling,
                          elementType, torusMajorR, torusMinorR, sphereRadius,
                          minLocal, maxLocal);

    return computeScreenSpaceSize(minLocal, maxLocal, elementPos, elementNormal,
                                  faceArea, userScaling, mvp);
}

// ============================================================================
// LOD Resolution Selection
// ============================================================================

// Maps screen-space size to a UV grid resolution.
// Formula: resolution = baseMN * sqrt(screenSize * lodFactor)
// sqrt() ensures resolution scales as the square root of screen area,
// keeping pixel density roughly constant.
uint computeLodResolution(float screenSize, uint baseMN, float lodFactor,
                          uint minResolution, uint maxResolution) {
    float target = float(baseMN) * sqrt(screenSize * lodFactor);
    uint resolution = uint(target);
    resolution = max(resolution, minResolution);
    resolution = min(resolution, maxResolution);
    return resolution;
}

// Compute adaptive M×N resolution from element world-space data.
void getLodMN(vec3 elementPos, vec3 elementNormal, float faceArea, float userScaling,
              uint elementType, float torusMajorR, float torusMinorR, float sphereRadius,
              mat4 mvp, uint baseMN, float lodFactor, uint minResolution, uint maxResolution,
              out uint outM, out uint outN) {
    float screenSize = getScreenSpaceSize(elementPos, elementNormal, faceArea, userScaling,
                                          elementType, torusMajorR, torusMinorR, sphereRadius, mvp);

    uint resolution = computeLodResolution(screenSize, baseMN, lodFactor, minResolution, maxResolution);
    outM = resolution;
    outN = resolution;
}

#endif // LODS_GLSL
