#version 460                                    // Use GLSL version 4.60
#extension GL_EXT_mesh_shader : require          // Enable mesh shader extension (for the perprimitiveEXT input below)
#extension GL_GOOGLE_include_directive : require // Enable #include directive

#define FRAGMENT_SHADER                          // Marks this as the fragment stage (used by shared headers)
#define PEBBLE_PIPELINE                          // Select the pebble UBO (pebbleUbo) variant in shaderInterface.h
#include "shaderInterface.h"                     // Shared set/binding defines, UBO/SSBO layouts, HE accessor helpers
#include "noise.glsl"                            // Noise helpers (included for parity; not used in this shader)
#include "shading.glsl"                          // PBR (cookTorrancePBR), tone mapping, getDebugColor

// =============================================================================
// pebble.frag — Pebble fragment shader
//
// Final stage of the pebble pipeline. Shades the subdivided pebble surface
// emitted by pebble.mesh. Branches on pc.debugMode to render normally
// (Cook-Torrance PBR with optional AO texture and ACES tone mapping) or draw a
// debug view (normals, UVs, per-face ID color, element type, or a wireframe
// overlay). Pebbles are always face elements, so several inputs are simpler
// than the parametric pipeline's.
// =============================================================================

// *** Michael Salton ***

// Per-vertex inputs (interpolated)
layout(location = 0) in vec4 worldPosU;          // World position (xyz) + local U coordinate (w)
layout(location = 1) in vec4 normalV;            // World normal (xyz) + local V coordinate (w)

// Per-primitive inputs (flat)
layout(location = 2) perprimitiveEXT in flat uvec4 primData; // x = base face ID; y,z,w unused

layout(set = SET_SCENE, binding = BINDING_VIEW_UBO) uniform ViewUBOBlock { // Scene-set UBO (set 0, binding 0): camera data
    mat4 view;                                   // World→view matrix
    mat4 projection;                             // View→clip matrix
    vec4 cameraPosition;                         // Camera world position (specular/view dir)
    float nearPlane;                             // Near clip distance
    float farPlane;                              // Far clip distance
} viewUBO;                                       // Instance name of the view UBO

layout(set = SET_SCENE, binding = BINDING_SHADING_UBO) uniform ShadingUBOBlock { // Scene-set UBO (set 0, binding 1): lighting + material
    vec4  lightPosition;                         // Light world position (w unused)
    vec4  ambient;                               // Ambient color (rgb) + intensity (a)
    float lightIntensity;                        // Light brightness multiplier
    float roughness;                             // Pebble roughness
    float metallic;                              // Pebble metallic
    float ao;                                    // Pebble ambient occlusion
    float dielectricF0;                          // Pebble dielectric F0
    float envReflection;                         // Pebble env reflection strength
    float _baseMeshRoughness;                    // (base-mesh material fields — unused here)
    float _baseMeshMetallic;                     // (unused here)
    float _baseMeshAo;                           // (unused here)
    float _baseMeshDielectricF0;                 // (unused here)
    float _baseMeshEnvReflection;                // (unused here)
    float _padding1;                             // std140 alignment padding
    vec4  procBaseColor;                         // Pebble base color (albedo)
} shadingUBO;                                    // Instance name of the shading UBO

layout(push_constant) uniform PushConstants {    // Per-draw constants (shared layout)
    mat4 model;                                  // Object→world transform (unused here)
    uint nbFaces;                                // Face count (unused here)
    uint nbVertices;                             // Vertex count (unused here)
    uint elementType;                            // Element type (unused here)
    float userScaling;                           // Global scale (unused here)
    float torusMajorR;                           // (unused here)
    float torusMinorR;                           // (unused here)
    float sphereRadius;                          // (unused here)
    uint resolutionM;                            // (unused here)
    uint resolutionN;                            // (unused here)
    uint debugMode;                              // Selects render vs. debug view
    uint enableCulling;                          // (unused here)
    float cullingThreshold;                      // (unused here)
    uint enableLod;                              // (unused here)
    float lodFactor;                             // (unused here)
    uint chainmailMode;                          // (unused here)
    float chainmailTiltAngle;                    // (unused here)
} pc;                                            // Instance name of the push-constant block

layout(location = 0) out vec4 outColor;          // Final fragment color

vec2 getBaseUv(uint faceId) {                    // Look up a face's base-mesh UV (from its first vertex)
    uint vertId = uint(getHalfEdgeVertex(uint(getFaceEdge(faceId)))); // First vertex of the face
    return getVertexTexCoord(vertId);            // Its texture coordinate
}

void main() {                                    // Fragment shader entry (once per covered pixel)
    vec3 worldPos = worldPosU.xyz;               // Unpack world-space position
    vec3 normal = normalize(normalV.xyz);        // Unpack and renormalize the world-space normal
    vec2 localUV = vec2(worldPosU.w, normalV.w); // Reassemble the pebble's local UV
    uint faceId = primData.x;                    // Source base-mesh face ID

    vec3 color;                                  // Output color accumulator

    switch (pc.debugMode) {                      // Branch on the active view mode
        case 0: {                                // ---- Mode 0: normal shaded render ----
            vec3 albedo = shadingUBO.procBaseColor.rgb; // Pebble base color

            color = cookTorrancePBR(worldPos, normal,    // Cook-Torrance PBR lighting
                                    shadingUBO.lightPosition.xyz,
                                    viewUBO.cameraPosition.xyz,
                                    albedo,
                                    shadingUBO.roughness,
                                    shadingUBO.metallic,
                                    shadingUBO.dielectricF0,
                                    shadingUBO.ambient,
                                    shadingUBO.envReflection,
                                    shadingUBO.lightIntensity);
            color *= shadingUBO.ao;              // Apply ambient occlusion

            // AO texture
            if (pebbleUbo.hasAOTexture != 0) {   // If a baked AO texture is bound...
                vec2 baseUV = getBaseUv(faceId); // ...sample at the face's base-mesh UV
                baseUV.y = 1.0 - baseUV.y;       // Flip V (OBJ convention)
                float aoTex = texture(sampler2D(textures[AO_TEXTURE], samplers[LINEAR_SAMPLER]), baseUV).r; // AO from red channel
                color *= aoTex;                  // Modulate by baked AO
            }

            color = toneMapACES(color);          // Tone-map HDR → LDR
            break;
        }

        case 1: {                                // ---- Mode 1: normal visualization ----
            // Normal visualization
            color = normal * 0.5 + 0.5;          // Remap normal [-1,1] → color [0,1]
            break;
        }

        case 2: {                                // ---- Mode 2: UV visualization ----
            // UV visualization
            color = vec3(localUV, 0.5);          // Show local UV as red/green, constant blue
            break;
        }

        case 3: {                                // ---- Mode 3: face ID visualization ----
            // Task ID (per-face unique color)
            color = getDebugColor(faceId);       // Hash the face ID → distinct color
            break;
        }

        case 4: {                                // ---- Mode 4: element type ----
            // Element type: red = vertex, blue = face
            // Pebbles are always face elements
            color = vec3(0.2, 0.2, 1);           // Always blue (pebbles never sit on vertices)
            break;
        }

        case 5: {                                // ---- Mode 5: wireframe overlay ----
            // Wireframe overlay using local UV grid
            uint N = pebbleUbo.subdivisionLevel; // Subdivision level → grid density
            vec2 gridUV = localUV * float(1u << N); // Scale UV to the subdivision grid (2^N cells)
            vec2 grid = abs(fract(gridUV - 0.5) - 0.5) / fwidth(gridUV); // Distance to nearest grid line (pixels)
            float line = min(grid.x, grid.y);    // Nearest line in either axis
            float wire = 1.0 - smoothstep(0.0, 1.5, line); // Anti-aliased wire mask

            // Base shading
            color = cookTorrancePBR(worldPos, normal, // Underlying lit surface
                                    shadingUBO.lightPosition.xyz,
                                    viewUBO.cameraPosition.xyz,
                                    shadingUBO.procBaseColor.rgb,
                                    shadingUBO.roughness,
                                    shadingUBO.metallic,
                                    shadingUBO.dielectricF0,
                                    shadingUBO.ambient,
                                    shadingUBO.envReflection,
                                    shadingUBO.lightIntensity);

            // White wireframe overlay
            color = mix(color, vec3(1.0), wire * 0.7); // Blend white wire on top
            color = toneMapACES(color);          // Tone-map
            break;
        }

        default: {                               // ---- Fallback: plain PBR ----
            color = cookTorrancePBR(worldPos, normal,
                                    shadingUBO.lightPosition.xyz,
                                    viewUBO.cameraPosition.xyz,
                                    shadingUBO.procBaseColor.rgb,
                                    shadingUBO.roughness,
                                    shadingUBO.metallic,
                                    shadingUBO.dielectricF0,
                                    shadingUBO.ambient,
                                    shadingUBO.envReflection,
                                    shadingUBO.lightIntensity);
            color = toneMapACES(color);          // Tone-map
            break;
        }
    }

    outColor = vec4(color, 1.0);                 // Write the final opaque color
}                                                // End of main()

// *** ************ ***
