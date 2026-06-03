#version 450                                    // Use GLSL version 4.50
#extension GL_EXT_mesh_shader : require          // Enable mesh shader extension (for the perprimitiveEXT inputs below)
#extension GL_GOOGLE_include_directive : require // Enable #include directive

#include "shaderInterface.h"                    // Shared set/binding defines, UBO/SSBO layouts, HE accessor helpers
#include "shading.glsl"                          // PBR (cookTorrancePBR), tone mapping, heatmap, getDebugColor, specular-AA helpers

// =============================================================================
// parametric.frag — Resurfacing fragment shader
//
// Final stage of the parametric resurfacing pipeline. Shades the tessellated
// procedural elements emitted by parametric.mesh. Selects the primary or
// secondary PBR material, applies optional geometric specular antialiasing,
// and branches on push.debugMode to either render normally (Cook-Torrance PBR,
// with chainmail-specific ambient occlusion and optional AO texture) or draw
// one of several debug visualizations (normals, UVs, task ID, element type,
// wireframe, curvature/feature/screen-size/proxy heatmaps). Outputs coverage-
// fade alpha for alpha-to-coverage when enabled.
// =============================================================================

// *** Michael Salton ***

// Per-vertex inputs (interpolated)
layout(location = 0) in PerVertexData {         // Interpolated data from the mesh shader
    vec4 worldPosU;  // xyz = world position, w = u coordinate // World position + U coordinate
    vec4 normalV;    // xyz = world normal, w = v coordinate    // World normal + V coordinate
} vIn;

// Per-primitive inputs (flat)
layout(location = 2) perprimitiveEXT in PerPrimitiveData { // Flat per-triangle identifiers
    flat uvec4 data;  // x = taskId, y = isVertex, z = elementType, w = faceId // Packed element IDs
} pIn;

layout(location = 3) perprimitiveEXT in vec2 baseUV;       // Base-mesh UV for this element (AO/texture sampling)
layout(location = 4) perprimitiveEXT in vec3 faceNormal;   // Base-surface normal (used by specular AA)
layout(location = 5) perprimitiveEXT in float screenAlpha; // Coverage-fade alpha for this element
layout(location = 6) perprimitiveEXT in float inCurvature; // GRWM curvature value (heatmap)

// UBOs
layout(set = SET_SCENE, binding = BINDING_VIEW_UBO) uniform ViewUBOBlock { // Scene-set UBO (set 0, binding 0): camera data
    mat4 view;                                  // World→view matrix
    mat4 projection;                            // View→clip matrix
    vec4 cameraPosition;                        // Camera world position (specular/view dir)
    float nearPlane;                            // Near clip distance
    float farPlane;                             // Far clip distance
} viewUBO;                                      // Instance name of the view UBO

layout(set = SET_SCENE, binding = BINDING_SHADING_UBO) uniform ShadingUBOBlock { // Scene-set UBO (set 0, binding 1): lighting + materials
    vec4  lightPosition;                        // Light world position (w unused)
    vec4  ambient;                              // Ambient color (rgb) + intensity (a)
    float lightIntensity;                       // Light brightness multiplier
    float roughness;                            // Primary element roughness
    float metallic;                             // Primary element metallic
    float ao;                                   // Primary element ambient occlusion
    float dielectricF0;                         // Primary element dielectric F0
    float envReflection;                        // Primary element env reflection strength
    float secondaryRoughness;                   // Secondary mesh roughness
    float secondaryMetallic;                    // Secondary mesh metallic
    float secondaryAo;                          // Secondary mesh ambient occlusion
    float secondaryDielectricF0;                // Secondary mesh dielectric F0
    float secondaryEnvReflection;               // Secondary mesh env reflection strength
    float _padding1;                            // std140 alignment padding
    vec4  procBaseColor;                        // Primary element base color
    vec4  secondaryBaseColor;                   // Secondary mesh base color
} shadingUBO;                                   // Instance name of the shading UBO

// Push constants (must match task/mesh layout)
layout(push_constant) uniform PushConstants {   // Per-draw constants (shared layout across task/mesh/frag)
    mat4 model;                                // Object→world transform
    uint nbFaces;                              // Face count (unused here)
    uint nbVertices;                           // Vertex count (unused here)
    uint elementType;                          // Default element type (unused here)
    float userScaling;                         // Global scale (unused here)
    float torusMajorR;                         // Torus major radius (unused here)
    float torusMinorR;                         // Torus minor radius (unused here)
    float sphereRadius;                        // Sphere radius (unused here)
    uint resolutionM;                          // Resolution U (used for the wireframe debug grid)
    uint resolutionN;                          // Resolution V (used for the wireframe debug grid)
    uint debugMode;     // 0=shading, 1=normals, 2=UV, 3=taskID, 4=element type (face/vertex) // Selects render vs. debug view
    uint enableCulling;                        // Culling bitmask (unused here)
    float cullingThreshold;                    // Cull threshold (unused here)
    uint enableLod;                            // LOD flag (unused here)
    float lodFactor;                           // LOD factor (unused here)
    uint chainmailMode;                        // 0 = off, 1 = chainmail (enables chainmail AO below)
    float chainmailTiltAngle;                  // Chainmail tilt (unused here)
    uint useDirectIndex;                       // Reused as the "is secondary mesh" flag (selects secondary material)
    float chainmailSurfaceOffset;              // Chainmail lift (unused here)
} push;                                        // Instance name of the push-constant block

layout(location = 0) out vec4 outColor;         // Final fragment color (rgb) + coverage alpha (a)

void main() {                                   // Fragment shader entry point (runs once per covered pixel)
    vec3 worldPos = vIn.worldPosU.xyz;          // Unpack world-space position
    vec3 normal = normalize(vIn.normalV.xyz);   // Unpack and renormalize the world-space normal
    vec2 uv = vec2(vIn.worldPosU.w, vIn.normalV.w); // Reassemble the element's parametric UV

    uint taskId = pIn.data.x;                   // Unique element ID (debug coloring / chainmail hash)
    uint isVertex = pIn.data.y;                 // 1 = vertex element, 0 = face element
    uint faceId = pIn.data.w;                   // Source face ID (proxy/feature buffer lookups)

    // Select PBR material: secondary mesh (useDirectIndex=1) uses the secondary fields
    bool isSecondary = (push.useDirectIndex != 0u); // Secondary mesh draws set useDirectIndex
    vec3  matBaseColor  = isSecondary ? shadingUBO.secondaryBaseColor.rgb  : shadingUBO.procBaseColor.rgb; // Pick base color
    float matRoughness  = isSecondary ? shadingUBO.secondaryRoughness      : shadingUBO.roughness;     // Pick roughness
    float matMetallic   = isSecondary ? shadingUBO.secondaryMetallic       : shadingUBO.metallic;      // Pick metallic
    float matAo         = isSecondary ? shadingUBO.secondaryAo             : shadingUBO.ao;            // Pick ambient occlusion
    float matF0         = isSecondary ? shadingUBO.secondaryDielectricF0   : shadingUBO.dielectricF0;  // Pick dielectric F0
    float matEnvRefl    = isSecondary ? shadingUBO.secondaryEnvReflection  : shadingUBO.envReflection; // Pick env reflection

    // Geometric specular antialiasing (Tokuyoshi & Kaplanyan 2021, extended for procedural geometry)
    float aaDebugValue = 0.0;                   // (Diagnostic: how much roughness AA added; not output)
    if (resurfacingUBO.enableSpecularAA != 0u) { // If geometric specular AA is enabled...
        float oldRoughness = matRoughness;      // Remember the pre-AA roughness
        matRoughness = filterRoughnessProceduralAA(normal, normalize(faceNormal), matRoughness, // Widen roughness based on
                                                         resurfacingUBO.specularAAStrengthUBO);  // sub-pixel normal variation
        aaDebugValue = matRoughness - oldRoughness;  // how much roughness was added
    }

    bool useEnvMap = (resurfacingUBO.hasEnvMap != 0u); // Whether an environment map is bound (IBL reflections)
    vec3 color;                                 // Accumulated output color

    switch (push.debugMode) {                   // Branch on the active view mode
        case 0: {                               // ---- Mode 0: normal shaded render ----
            if (push.chainmailMode != 0u) {     // Chainmail: PBR plus ring-specific ambient occlusion
                color = cookTorrancePBR(worldPos, normal, // Base PBR lighting
                                        shadingUBO.lightPosition.xyz,
                                        viewUBO.cameraPosition.xyz,
                                        matBaseColor,
                                        matRoughness,
                                        matMetallic,
                                        matF0,
                                        shadingUBO.ambient,
                                        matEnvRefl,
                                        shadingUBO.lightIntensity,
                                        useEnvMap);
                color *= matAo;                 // Apply base material AO

                // --- Chainmail-specific AO modifiers ---
                // v=0 is outer top of torus, v=0.5 is inner bottom (closest to mesh surface)
                float v = fract(uv.y);          // Ring V coordinate, wrapped to [0,1)
                // Inner face darkening: strongest at v=0.5 (bottom of ring)
                float innerFace = 1.0 - 0.6 * pow(1.0 - abs(v * 2.0 - 1.0), 2.0); // Darken the ring's inner/underside

                // Edge AO: darken near UV boundaries (where rings interlock)
                float edgeU = min(uv.x, 1.0 - uv.x) * 2.0; // Distance to nearest U edge (0 at edges, 1 at center)
                float edgeV = min(v, 1.0 - v) * 2.0;       // Distance to nearest V edge
                float edgeAO = mix(0.7, 1.0, smoothstep(0.0, 0.15, edgeU)); // Darken toward U edges
                edgeAO *= mix(0.8, 1.0, smoothstep(0.0, 0.1, edgeV));       // Darken toward V edges

                // Self-shadow: fragments facing away from light get extra darkening
                vec3 N = normalize(normal);     // Surface normal
                vec3 L = normalize(shadingUBO.lightPosition.xyz - worldPos); // Direction to light
                float NdotL = max(dot(N, L), 0.0); // Lambert term
                float selfShadow = mix(0.35, 1.0, smoothstep(-0.1, 0.4, NdotL)); // Darken back-facing fragments

                // Per-ring brightness variation using taskId hash
                float ringHash = fract(float(taskId) * 0.618033988749895 + float(taskId * 7u) * 0.3819); // Pseudo-random per ring
                float ringVariation = mix(0.82, 1.0, ringHash); // Slight per-ring brightness jitter

                float occlusion = innerFace * edgeAO * selfShadow * ringVariation; // Combine all AO factors
                color *= occlusion;             // Apply combined chainmail occlusion
            } else {                            // Non-chainmail: plain PBR
                // Standard PBR with per-mesh material selection
                color = cookTorrancePBR(worldPos, normal,
                                        shadingUBO.lightPosition.xyz,
                                        viewUBO.cameraPosition.xyz,
                                        matBaseColor,
                                        matRoughness,
                                        matMetallic,
                                        matF0,
                                        shadingUBO.ambient,
                                        matEnvRefl,
                                        shadingUBO.lightIntensity,
                                        useEnvMap);
                color *= matAo;                 // Apply base material AO
            }

            // Apply ambient occlusion from texture
            if (resurfacingUBO.hasAOTexture != 0u) { // If an AO texture is bound...
                vec2 aoUV = baseUV;             // Sample at the element's base-mesh UV
                aoUV.y = 1.0 - aoUV.y;  // Flip V (OBJ convention)
                float aoTex = texture(sampler2D(textures[AO_TEXTURE], samplers[LINEAR_SAMPLER]), aoUV).r; // AO from red channel
                color *= aoTex;                 // Modulate color by baked AO
            }

            color = toneMapACES(color);         // Tone-map HDR → LDR
            break;
        }

        case 1: {                               // ---- Mode 1: normal visualization ----
            // Normal visualization
            color = normal * 0.5 + 0.5;         // Remap normal [-1,1] → color [0,1]
            break;
        }

        case 2: {                               // ---- Mode 2: UV visualization ----
            // UV coordinate visualization
            color = vec3(uv, 0.5);              // Show UV as red/green, constant blue
            break;
        }

        case 3: {                               // ---- Mode 3: task ID visualization ----
            // Task ID visualization (unique color per element)
            color = getDebugColor(taskId);      // Hash element ID → distinct color
            break;
        }

        case 4: {                               // ---- Mode 4: element type ----
            // Element type: red = vertex, blue = face
            color = isVertex == 1 ? vec3(1, 0.2, 0.2) : vec3(0.2, 0.2, 1); // Red for vertex elements, blue for face elements
            break;
        }

        case 5: {                               // ---- Mode 5: wireframe overlay ----
            // Wireframe overlay: use UV grid lines scaled by resolution
            vec2 gridUV = uv * vec2(push.resolutionM, push.resolutionN); // Scale UV to the tessellation grid
            vec2 grid = abs(fract(gridUV - 0.5) - 0.5) / fwidth(gridUV); // Distance to nearest grid line (in pixels)
            float line = min(grid.x, grid.y);   // Nearest line in either axis
            float wire = 1.0 - smoothstep(0.0, 1.5, line); // Anti-aliased subdivision wire mask

            // Base shading
            color = cookTorrancePBR(worldPos, normal, // Underlying lit surface
                                    shadingUBO.lightPosition.xyz,
                                    viewUBO.cameraPosition.xyz,
                                    matBaseColor,
                                    matRoughness,
                                    matMetallic,
                                    matF0,
                                    shadingUBO.ambient,
                                    matEnvRefl,
                                    shadingUBO.lightIntensity);

            // Also draw element boundaries
            vec2 elemEdge = abs(fract(uv - 0.5) - 0.5) / fwidth(uv); // Distance to the element's [0,1] UV boundary
            float elemLine = min(elemEdge.x, elemEdge.y); // Nearest element boundary line
            float elemWire = 1.0 - smoothstep(0.0, 1.5, elemLine); // Anti-aliased element-boundary mask

            // White wireframe for subdivisions, yellow for element boundaries
            color = mix(color, vec3(1.0), wire * 0.7); // Overlay white subdivision wires
            color = mix(color, vec3(1.0, 0.9, 0.2), elemWire * 0.9); // Overlay yellow element borders
            color = toneMapACES(color);         // Tone-map
            break;
        }

        case 6: {                               // ---- Mode 6: curvature heatmap ----
            // Curvature heatmap
            float curv = inCurvature * resurfacingUBO.preprocessCurvatureScale; // Normalize curvature
            color = heatmap(clamp(curv, 0.0, 1.0)); // Map [0,1] → heatmap color
            break;
        }

        case 7: {                               // ---- Mode 7: feature-edge heatmap ----
            // Feature edge heatmap
            uint feat = (resurfacingUBO.hasPreprocessData != 0u) ? getFaceFeatureFlag(faceId) : 0u; // Per-face feature flag
            color = (feat != 0u) ? vec3(1.0, 0.2, 0.1) : vec3(0.1, 0.3, 1.0); // Red if feature edge, blue otherwise
            break;
        }

        case 8: {                               // ---- Mode 8: screen-size heatmap ----
            // Screen size heatmap (green = large, red = sub-pixel)
            color = heatmap(1.0 - screenAlpha); // Invert coverage alpha so sub-pixel elements read hot
            break;
        }

        case 9: {                               // ---- Mode 9: proxy-blend heatmap ----
            // Proxy blend heatmap
            float blend = 0.0;                  // Default when proxy mode off
            if (resurfacingUBO.enableProxy != 0u) { // If proxy mode active...
                blend = heProxyBuffer[0].data[faceId].blend; // ...read this face's proxy blend factor
            }
            color = heatmap(blend);             // Map blend → heatmap color
            break;
        }

        default: {                              // ---- Fallback: plain PBR ----
            color = cookTorrancePBR(worldPos, normal,
                                    shadingUBO.lightPosition.xyz,
                                    viewUBO.cameraPosition.xyz,
                                    matBaseColor,
                                    matRoughness,
                                    matMetallic,
                                    matF0,
                                    shadingUBO.ambient,
                                    matEnvRefl,
                                    shadingUBO.lightIntensity);
            color = toneMapACES(color);         // Tone-map
            break;
        }
    }

    // Coverage fade via alpha-to-coverage (requires MSAA)
    float alpha = (resurfacingUBO.enableCoverageFade != 0u) ? screenAlpha : 1.0; // Sub-pixel fade alpha, else fully opaque
    outColor = vec4(color, alpha);              // Write final color + coverage alpha
}                                               // End of main()

// *** ************ ***
