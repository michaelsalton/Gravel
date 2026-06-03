#ifndef CULLING_GLSL                             // Include guard: skip if already included this compilation unit
#define CULLING_GLSL

// =============================================================================
// culling.glsl — Frustum and back-face culling helpers
//
// Visibility tests used by the task shaders to skip elements before they are
// tessellated. isInFrustum does a conservative bounding-sphere test in NDC
// after clip-space projection; computeBoundingRadius derives a sphere radius
// from a face's area; isFrontFacing rejects elements whose normal points away
// from the camera.
// =============================================================================

// *** Michael Salton ***

// ============================================================================
// Frustum Culling
// ============================================================================

// Check if a bounding sphere is inside or intersecting the view frustum
// Uses NDC test after clip-space projection
bool isInFrustum(vec3 worldPos, float radius, mat4 mvp, float margin) { // True if the sphere is potentially visible
    vec4 clipPos = mvp * vec4(worldPos, 1.0);    // Project the sphere center to clip space

    // Behind near plane
    if (clipPos.w <= 0.0) {                      // w ≤ 0 means the center is behind the camera...
        return false;                            // ...cull
    }

    // Conservative radius in NDC space
    float clipRadius = radius / clipPos.w * 2.0; // Approximate the sphere radius in NDC (perspective scale)
    float expandedRadius = clipRadius * (1.0 + margin); // Add a safety margin to stay conservative

    vec3 ndc = clipPos.xyz / clipPos.w;          // Perspective divide → NDC center

    // Left/right planes
    if (ndc.x + expandedRadius < -1.0 || ndc.x - expandedRadius > 1.0) { // Fully left of or right of the frustum...
        return false;                            // ...cull
    }

    // Bottom/top planes
    if (ndc.y + expandedRadius < -1.0 || ndc.y - expandedRadius > 1.0) { // Fully below or above...
        return false;                            // ...cull
    }

    // Near/far planes (Vulkan: [0, 1])
    if (ndc.z + expandedRadius < 0.0 || ndc.z - expandedRadius > 1.0) { // Fully in front of near or beyond far...
        return false;                            // ...cull (Vulkan depth range is [0,1])
    }

    return true;                                 // Otherwise the sphere overlaps the frustum → keep
}

// Conservative bounding radius from face area and scaling
float computeBoundingRadius(float faceArea, float userScaling, float surfaceMargin) { // Sphere radius for an element
    float baseRadius = sqrt(faceArea / 3.14159265359); // Radius of a circle with the same area as the face
    return baseRadius * userScaling * surfaceMargin;    // Scale by the global size and an extra margin
}

// ============================================================================
// Back-Face Culling
// ============================================================================

// Check if element normal faces toward camera
// viewDir dot normal > threshold means front-facing
bool isFrontFacing(vec3 worldPos, vec3 normal, vec3 cameraPos, float threshold) { // True if the element faces the camera
    vec3 viewDir = normalize(cameraPos - worldPos); // Direction from the element toward the camera
    float facing = dot(viewDir, normal);         // Alignment of the normal with the view direction
    return facing > threshold;                   // Keep only if it faces the camera beyond the threshold
}

#endif // CULLING_GLSL

// *** ************ ***
