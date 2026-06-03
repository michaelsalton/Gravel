#version 450                                   // Use GLSL version 4.50
#extension GL_EXT_mesh_shader : require         // Enable mesh shader extension (needed for the perprimitiveEXT input below)
#extension GL_GOOGLE_include_directive : require // Enable #include directive; required (hard error if unsupported)

#include "shaderInterface.h"                    // Shared set/binding defines, UBO/SSBO layouts, and HE accessor helpers
#include "shading.glsl"                         // PBR helpers: cookTorrancePBR(), toneMapACES(), heatmap()

// =============================================================================
// basemesh.frag — Base-mesh fragment shader
//
// Shades the flat-shaded triangles emitted by basemesh.mesh. Has two roles:
//   1. Debug visualizations selected by push.debugMode (mask/skin previews,
//      per-face hash coloring, curvature/feature/proxy heatmaps).
//   2. Normal rendering: selects the primary or secondary base-mesh material,
//      applies optional diffuse/normal/ORM textures, runs Cook-Torrance PBR
//      lighting, blends toward an aggregate "proxy" look for sub-pixel faces,
//      and ACES tone-maps the result.
// =============================================================================

// *** Michael Salton ***

layout(location = 0) in vec3 inNormal;          // Interpolated world-space normal from the mesh shader
layout(location = 1) in vec2 inUV;              // Interpolated texture coordinates
layout(location = 2) perprimitiveEXT in flat uint inFaceId; // Per-primitive (non-interpolated) source face index from the mesh shader
layout(location = 3) in vec3 inWorldPos;        // Interpolated world-space surface position

layout(set = SET_SCENE, binding = BINDING_VIEW_UBO) uniform ViewUBOBlock { // Scene-set UBO (set 0, binding 0): per-frame camera data
    mat4 view;                                  // World→view (camera) matrix
    mat4 projection;                            // View→clip (projection) matrix
    vec4 cameraPosition;                        // Camera world-space position (used for specular/view direction)
    float nearPlane;                            // Camera near clip plane distance
    float farPlane;                             // Camera far clip plane distance
} viewUBO;                                      // Instance name of the view UBO

layout(set = SET_SCENE, binding = BINDING_SHADING_UBO) uniform ShadingUBOBlock { // Scene-set UBO (set 0, binding 1): lighting + materials
    vec4  lightPosition;                        // World-space light position (w unused)
    vec4  ambient;                              // Ambient color (rgb) and intensity (a)
    float lightIntensity;                       // Scalar light brightness multiplier
    float _procRoughness;       // offset 36    // Procedural element roughness (used for proxy shading below)
    float _procMetallic;                        // Procedural element metallic
    float _procAo;                              // Procedural element ambient occlusion
    float _procDielectricF0;                    // Procedural element dielectric base reflectance (F0)
    float _procEnvReflection;                   // Procedural element environment reflection strength
    float _secRoughness;        // offset 56    // (Secondary procedural material — unused in this shader)
    float _secMetallic;                         // (unused here)
    float _secAo;                               // (unused here)
    float _secDielectricF0;                     // (unused here)
    float _secEnvReflection;                    // (unused here)
    float _padding1;            // offset 76    // std140 alignment padding
    vec4  _procBaseColor;       // offset 80    // Procedural element base color (used for proxy shading)
    vec4  _secBaseColor;        // offset 96    // (Secondary procedural base color — unused here)
    float roughness;            // offset 112 — primary base mesh solid overlay material // Primary base-mesh roughness
    float metallic;                             // Primary base-mesh metallic
    float ao;                                   // Primary base-mesh ambient occlusion
    float dielectricF0;                         // Primary base-mesh dielectric F0
    float envReflection;                        // Primary base-mesh environment reflection strength
    float _pad2a; float _pad2b; float _pad2c;  // offset 132-140 // std140 alignment padding
    vec4  baseMeshBaseColor;    // offset 144   // Primary base-mesh base color
    float secRoughness;         // offset 160 — secondary base mesh solid overlay material // Secondary base-mesh roughness
    float secMetallic;                          // Secondary base-mesh metallic
    float secAo;                                // Secondary base-mesh ambient occlusion
    float secDielectricF0;                      // Secondary base-mesh dielectric F0
    float secEnvReflection;                     // Secondary base-mesh environment reflection strength
    float _pad3a; float _pad3b; float _pad3c;  // offset 180-188 // std140 alignment padding
    vec4  secBaseMeshBaseColor; // offset 192   // Secondary base-mesh base color
} shadingUBO;                                   // Instance name of the shading UBO

layout(push_constant) uniform PushConstants {   // Push-constant block: small per-draw data (shared struct across pipelines)
    mat4 model;                                 // Model (object→world) matrix
    uint nbFaces;                               // Total face count (unused here)
    uint nbVertices;                            // Total vertex count (unused here)
    uint elementType;                           // Resurfacing element type (unused here)
    float userScaling;                          // Global scale (unused here)
    float torusMajorR;                          // Torus major radius (unused here)
    float torusMinorR;                          // Torus minor radius (unused here)
    float sphereRadius;                         // Sphere radius (unused here)
    uint resolutionM;                           // U resolution (unused here)
    uint resolutionN;                           // V resolution (unused here)
    uint debugMode;                             // Debug visualization mode selector (drives the branches below)
    uint enableCulling;                         // Culling bitmask (unused here)
    float cullingThreshold;                     // Backface cull threshold (unused here)
    uint enableLod;                             // LOD enable flag (unused here)
    float lodFactor;                            // LOD factor (unused here)
    uint chainmailMode;                         // Chainmail mode (unused here)
    float chainmailTiltAngle;                   // Chainmail tilt angle (unused here)
    uint useDirectIndex;                        // Reused here as the "is secondary mesh" flag (selects secondary material)
} push;                                         // Instance name of the push-constant block

layout(location = 0) out vec4 outColor;         // Final fragment color written to the color attachment

void main() {                                   // Fragment shader entry point (runs once per covered pixel)
    vec3 N = normalize(inNormal);               // Re-normalize the interpolated normal (interpolation shortens it)

    // Mask preview: only when debugMode == 100 (Mask display mode)
    if (push.debugMode == 100u) {               // Debug mode 100: visualize the face-culling mask texture
        float maskVal = texture(                // Sample the mask texture...
            sampler2D(textures[MASK_TEXTURE], samplers[NEAREST_SAMPLER]), // ...combining mask image with the nearest-neighbor sampler
            inUV                                // ...at this fragment's UV
        ).r;                                    // Take the red channel as the mask value
        outColor = vec4(vec3(maskVal), 1.0);    // Output the mask as grayscale
        return;                                 // Done — skip normal shading
    }

    // Skin texture preview: debugMode == 101 (Skin display mode)
    if (push.debugMode == 101u) {               // Debug mode 101: shade using the skin texture as base color
        vec3 skinColor = texture(               // Sample the skin texture...
            sampler2D(textures[SKIN_TEXTURE], samplers[LINEAR_SAMPLER]), // ...with the linear sampler
            inUV                                // ...at this fragment's UV
        ).rgb;                                  // Take the RGB color
        vec3 color = cookTorrancePBR(inWorldPos, N, // Run PBR lighting using the skin color as albedo
                                     shadingUBO.lightPosition.xyz, // Light world position
                                     viewUBO.cameraPosition.xyz,   // Camera world position
                                     skinColor,                    // Base color (albedo)
                                     shadingUBO.roughness,         // Roughness
                                     shadingUBO.metallic,          // Metallic
                                     shadingUBO.dielectricF0,      // Dielectric F0
                                     shadingUBO.ambient,           // Ambient term
                                     shadingUBO.envReflection,     // Env reflection strength
                                     shadingUBO.lightIntensity);   // Light intensity
        color = toneMapACES(color);             // Apply ACES tone mapping (HDR → displayable LDR)
        outColor = vec4(color, 1.0);            // Output the shaded color
        return;                                 // Done
    }

    // Colored faces: unique color per face from hash
    if (push.debugMode == 102u) {               // Debug mode 102: give each face a distinct flat color
        uint h = inFaceId;                      // Seed the hash with the face ID
        h = ((h >> 16u) ^ h) * 0x45d9f3bu;      // Integer hash mix step 1 (xorshift + multiply)
        h = ((h >> 16u) ^ h) * 0x45d9f3bu;      // Integer hash mix step 2
        h = (h >> 16u) ^ h;                     // Final avalanche to spread bits
        vec3 faceColor = vec3(                  // Build an RGB color from the hashed bits:
            float((h >>  0u) & 0xFFu) / 255.0,  //   red   = low byte, normalized to [0,1]
            float((h >>  8u) & 0xFFu) / 255.0,  //   green = middle byte
            float((h >> 16u) & 0xFFu) / 255.0   //   blue  = high byte
        );
        // Light shading so faces are distinguishable in 3D
        float NdotL = max(dot(N, normalize(shadingUBO.lightPosition.xyz - inWorldPos)), 0.0); // Diffuse term (clamped N·L)
        outColor = vec4(faceColor * (0.3 + 0.7 * NdotL), 1.0); // Modulate face color with simple lambert + ambient floor
        return;                                 // Done
    }

    // Heatmap visualizations (modes 6-9)
    if (push.debugMode == 6u) {                 // Debug mode 6: curvature heatmap (from GRWM preprocess data)
        // Curvature heatmap: use first vertex of face from vertex-face index
        float curv = 0.0;                       // Default curvature when no preprocess data is loaded
        if (resurfacingUBO.hasPreprocessData != 0u) { // Only valid if GRWM curvature data was uploaded
            int offset = getFaceOffset(inFaceId); // Find this face's start in the vertex-index buffer
            int vertId = getVertexFaceIndex(offset); // Grab the face's first vertex index
            curv = abs(getVertexCurvature(uint(vertId))) * resurfacingUBO.preprocessCurvatureScale; // Curvature magnitude, normalized
        }
        float NdotL = max(dot(N, normalize(shadingUBO.lightPosition.xyz - inWorldPos)), 0.0); // Diffuse term for 3D readability
        outColor = vec4(heatmap(clamp(curv, 0.0, 1.0)) * (0.3 + 0.7 * NdotL), 1.0); // Map curvature [0,1] → heatmap color, lit
        return;                                 // Done
    }
    if (push.debugMode == 7u) {                 // Debug mode 7: feature-edge heatmap
        // Feature edge heatmap
        uint feat = (resurfacingUBO.hasPreprocessData != 0u) ? getFaceFeatureFlag(inFaceId) : 0u; // Per-face feature flag (0 if no data)
        vec3 c = (feat != 0u) ? vec3(1.0, 0.2, 0.1) : vec3(0.1, 0.3, 1.0); // Red if feature edge, blue otherwise
        float NdotL = max(dot(N, normalize(shadingUBO.lightPosition.xyz - inWorldPos)), 0.0); // Diffuse term
        outColor = vec4(c * (0.3 + 0.7 * NdotL), 1.0); // Output the lit feature color
        return;                                 // Done
    }
    if (push.debugMode == 8u) {                 // Debug mode 8: screen-size heatmap
        // Screen size: base mesh doesn't have screen alpha, show neutral
        outColor = vec4(vec3(0.5), 1.0);        // Base mesh has no per-element screen size → output flat gray
        return;                                 // Done
    }
    if (push.debugMode == 9u) {                 // Debug mode 9: proxy-blend heatmap
        // Proxy blend heatmap
        float blend = 0.0;                      // Default blend when proxy shading is disabled
        if (resurfacingUBO.enableProxy != 0u) { // Only read proxy buffer if proxy mode is active
            blend = heProxyBuffer[0].data[inFaceId].blend; // Per-face proxy blend factor [0,1]
        }
        float NdotL = max(dot(N, normalize(shadingUBO.lightPosition.xyz - inWorldPos)), 0.0); // Diffuse term
        outColor = vec4(heatmap(blend) * (0.3 + 0.7 * NdotL), 1.0); // Map blend → heatmap color, lit
        return;                                 // Done
    }

    // Select primary or secondary base mesh material
    bool isSecondary = (push.useDirectIndex != 0u); // Secondary mesh draws set useDirectIndex; pick the secondary material set
    vec3  matBaseColor = isSecondary ? shadingUBO.secBaseMeshBaseColor.rgb : shadingUBO.baseMeshBaseColor.rgb; // Choose base color

    // Apply diffuse texture if loaded
    if (resurfacingUBO.hasDiffuseTexture != 0u && !isSecondary) { // Primary mesh only: modulate by diffuse texture if present
        vec2 texUV = inUV;                      // Copy UV for flipping
        texUV.y = 1.0 - texUV.y;  // Flip V (OBJ convention) // OBJ textures use bottom-left origin; flip V to match
        vec3 texColor = texture(sampler2D(textures[DIFFUSE_TEXTURE], samplers[LINEAR_SAMPLER]), texUV).rgb; // Sample diffuse
        matBaseColor *= texColor;               // Tint the base color by the texture
    }
    // Normal mapping: construct tangent frame from screen-space derivatives
    if (resurfacingUBO.hasNormalTexture != 0u && !isSecondary) { // Primary mesh only: perturb normal from a normal map
        vec2 texUV = inUV;                      // Copy UV
        texUV.y = 1.0 - texUV.y;                // Flip V (OBJ convention)
        vec3 tangentNormal = texture(sampler2D(textures[NORMAL_TEXTURE], samplers[LINEAR_SAMPLER]), texUV).rgb; // Sample normal map
        tangentNormal = tangentNormal * 2.0 - 1.0; // Decode from [0,1] color range to [-1,1] direction

        vec3 dPdx = dFdx(inWorldPos);           // World-position derivative across screen-x
        vec3 dPdy = dFdy(inWorldPos);           // World-position derivative across screen-y
        vec2 dUVdx = dFdx(texUV);               // UV derivative across screen-x
        vec2 dUVdy = dFdy(texUV);               // UV derivative across screen-y

        float invDet = 1.0 / (dUVdx.x * dUVdy.y - dUVdx.y * dUVdy.x + 1e-8); // Inverse determinant of the UV Jacobian (epsilon avoids /0)
        vec3 T = (dPdx * dUVdy.y - dPdy * dUVdx.y) * invDet; // Solve for the tangent vector (∂P/∂u)
        vec3 B = (dPdy * dUVdx.x - dPdx * dUVdy.x) * invDet; // Solve for the bitangent vector (∂P/∂v)

        T = normalize(T - N * dot(N, T));       // Gram-Schmidt: orthogonalize T against N, then normalize
        B = cross(N, T);                        // Rebuild B orthogonal to both N and T

        N = normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z); // Transform mapped normal into world space
    }

    float matRoughness = isSecondary ? shadingUBO.secRoughness   : shadingUBO.roughness;     // Pick roughness for active material
    float matMetallic  = isSecondary ? shadingUBO.secMetallic    : shadingUBO.metallic;      // Pick metallic
    float matAo        = isSecondary ? shadingUBO.secAo          : shadingUBO.ao;            // Pick ambient occlusion
    float matF0        = isSecondary ? shadingUBO.secDielectricF0 : shadingUBO.dielectricF0;  // Pick dielectric F0
    float matEnvRefl   = isSecondary ? shadingUBO.secEnvReflection : shadingUBO.envReflection; // Pick env reflection strength

    // ORM texture: R=occlusion, G=roughness, B=metallic
    if (resurfacingUBO.hasOrmTexture != 0u && !isSecondary) { // Primary mesh only: override material from packed ORM map
        vec2 texUV = inUV;                      // Copy UV
        texUV.y = 1.0 - texUV.y;                // Flip V (OBJ convention)
        vec3 orm = texture(sampler2D(textures[ORM_TEXTURE], samplers[LINEAR_SAMPLER]), texUV).rgb; // Sample ORM texture
        matAo *= orm.r;                         // R channel scales ambient occlusion
        matRoughness = orm.g;                   // G channel sets roughness
        matMetallic = orm.b;                    // B channel sets metallic
    }

    bool useEnvMap = (resurfacingUBO.hasEnvMap != 0u); // Whether an environment map is bound (enables IBL reflections)

    vec3 color = cookTorrancePBR(inWorldPos, N, // Main lighting: Cook-Torrance PBR for this fragment
                                 shadingUBO.lightPosition.xyz, // Light world position
                                 viewUBO.cameraPosition.xyz,   // Camera world position
                                 matBaseColor,                 // Base color (albedo)
                                 matRoughness,                 // Roughness
                                 matMetallic,                  // Metallic
                                 matF0,                        // Dielectric F0
                                 shadingUBO.ambient,           // Ambient term
                                 matEnvRefl,                   // Env reflection strength
                                 shadingUBO.lightIntensity,    // Light intensity
                                 useEnvMap);                   // Use env map for reflections?

    // Proxy shading: blend with aggregate procedural appearance for sub-pixel faces
    ProxyFaceData pd = heProxyBuffer[0].data[inFaceId]; // Read this face's proxy data (blend factor + flags)
    if (pd.blend > 0.0) {                       // If the face is small enough to use proxy (aggregate) shading...
        // Use the procedural element's material with widened roughness
        vec3 procColor = shadingUBO._procBaseColor.rgb; // Aggregate procedural base color
        float procRoughness = clamp(shadingUBO._procRoughness + 0.3, 0.0, 1.0); // aggregate roughness boost // Widen roughness for the blurred aggregate look
        float procMetallic = shadingUBO._procMetallic; // Aggregate procedural metallic
        float procF0 = shadingUBO._procDielectricF0;    // Aggregate procedural F0
        float procEnvRefl = shadingUBO._procEnvReflection; // Aggregate procedural env reflection

        // Self-shadow darkening: procedural elements partially occlude themselves
        procColor *= 0.7;  // approximate self-shadow scale // Darken to fake self-occlusion of dense elements

        vec3 proxyColor = cookTorrancePBR(inWorldPos, N, // Shade the aggregate proxy appearance with PBR
                                           shadingUBO.lightPosition.xyz, // Light world position
                                           viewUBO.cameraPosition.xyz,   // Camera world position
                                           procColor,                    // Proxy base color
                                           procRoughness,                // Proxy roughness
                                           procMetallic,                 // Proxy metallic
                                           procF0,                       // Proxy F0
                                           shadingUBO.ambient,           // Ambient term
                                           procEnvRefl,                  // Proxy env reflection
                                           shadingUBO.lightIntensity,    // Light intensity
                                           useEnvMap);                   // Use env map?

        color = mix(color, proxyColor, pd.blend); // Blend base-mesh color → proxy color by the proxy blend factor
    }

    color = toneMapACES(color);                 // Apply ACES tone mapping (HDR → displayable LDR)

    outColor = vec4(color, 1.0);                // Write the final opaque color
}                                               // End of main()

// *** ************ ***
