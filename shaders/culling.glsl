#ifndef CULLING_GLSL
#define CULLING_GLSL

// ============================================================================
// Frustum Culling
// ============================================================================

// Check if a bounding sphere is inside or intersecting the view frustum
// Uses NDC test after clip-space projection
bool isInFrustum(vec3 worldPos, float radius, mat4 mvp, float margin) {
    vec4 clipPos = mvp * vec4(worldPos, 1.0);

    // Behind near plane
    if (clipPos.w <= 0.0) {
        return false;
    }

    // Conservative radius in NDC space
    float clipRadius = radius / clipPos.w * 2.0;
    float expandedRadius = clipRadius * (1.0 + margin);

    vec3 ndc = clipPos.xyz / clipPos.w;

    // Left/right planes
    if (ndc.x + expandedRadius < -1.0 || ndc.x - expandedRadius > 1.0) {
        return false;
    }

    // Bottom/top planes
    if (ndc.y + expandedRadius < -1.0 || ndc.y - expandedRadius > 1.0) {
        return false;
    }

    // Near/far planes (Vulkan: [0, 1])
    if (ndc.z + expandedRadius < 0.0 || ndc.z - expandedRadius > 1.0) {
        return false;
    }

    return true;
}

// Conservative bounding radius from face area and scaling
float computeBoundingRadius(float faceArea, float userScaling, float surfaceMargin) {
    float baseRadius = sqrt(faceArea / 3.14159265359);
    return baseRadius * userScaling * surfaceMargin;
}

#endif // CULLING_GLSL
