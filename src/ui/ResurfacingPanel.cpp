#include "ui/ResurfacingPanel.h"
#include "renderer/renderer.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

void ResurfacingPanel::render(Renderer& r) {
    if (ImGui::CollapsingHeader("Resurfacing", ImGuiTreeNodeFlags_DefaultOpen)) {

        // Pipeline mode: None / Parametric / Pebble
        const char* pipelineModes[] = {"None", "Parametric", "Pebble"};
        int currentMode = 0;
        if (r.renderPebbles) currentMode = 2;
        else if (r.renderResurfacing) currentMode = 1;

        if (ImGui::Combo("Pipeline Mode", &currentMode, pipelineModes, 3)) {
            r.renderResurfacing = (currentMode == 1);
            r.renderPebbles = (currentMode == 2);
        }

        // ==================== Parametric Controls ====================
        if (r.renderResurfacing && !r.renderPebbles) {
            ImGui::Separator();

            const char* surfaceTypes[] = {"Torus", "Sphere", "Cone", "Cylinder"};
            int currentType = static_cast<int>(r.elementType);
            if (ImGui::Combo("Surface Type", &currentType, surfaceTypes, 4)) {
                r.elementType = static_cast<uint32_t>(currentType);
            }

            if (r.elementTypeTextureLoaded)
                ImGui::Checkbox("Use Element Type Map", &r.useElementTypeTexture);
            if (r.aoTextureLoaded)
                ImGui::Checkbox("Use AO Texture", &r.useAOTexture);
            if (r.maskTextureLoaded) {
                ImGui::Checkbox("Use Mask Texture", &r.useMaskTexture);
                if (!r.loadedMeshPath.empty()) {
                    auto dir = std::filesystem::path(r.loadedMeshPath).parent_path();
                    ImGui::TextDisabled("  %s/mask.png  (%ux%u)",
                        dir.filename().string().c_str(),
                        r.cpuMaskWidth, r.cpuMaskHeight);
                }
            }

            ImGui::SliderFloat("Global Scale", &r.userScaling, 0.01f, 3.0f);

            ImGui::Separator();

            if (r.elementType == 0) {
                ImGui::Text("Torus Parameters:");
                ImGui::SliderFloat("Major Radius", &r.torusMajorR, 0.3f, 2.0f);
                ImGui::SliderFloat("Minor Radius", &r.torusMinorR, 0.05f, 1.0f);
            } else if (r.elementType == 1) {
                ImGui::Text("Sphere Parameters:");
                ImGui::SliderFloat("Radius", &r.sphereRadius, 0.1f, 2.0f);
            }

            ImGui::Separator();

            // Resolution controls
            ImGui::Text("UV Grid Resolution:");
            int resM = static_cast<int>(r.resolutionM);
            int resN = static_cast<int>(r.resolutionN);
            if (ImGui::SliderInt("Resolution M", &resM, 2, 64)) {
                r.resolutionM = static_cast<uint32_t>(resM);
            }
            if (ImGui::SliderInt("Resolution N", &resN, 2, 64)) {
                r.resolutionN = static_cast<uint32_t>(resN);
            }

            // Tile info
            uint32_t deltaU = r.resolutionM, deltaV = r.resolutionN;
            if ((deltaU + 1) * (deltaV + 1) > 256) {
                uint32_t maxD = static_cast<uint32_t>(std::sqrt(256.0f)) - 1;
                deltaU = std::min(deltaU, maxD);
                deltaV = std::min(deltaV, maxD);
            }
            if (deltaU * deltaV * 2 > 256) {
                uint32_t maxD = static_cast<uint32_t>(std::sqrt(256.0f / 2.0f));
                deltaU = std::min(deltaU, maxD);
                deltaV = std::min(deltaV, maxD);
            }
            deltaU = std::max(deltaU, 2u);
            deltaV = std::max(deltaV, 2u);

            uint32_t numTilesU = (r.resolutionM + deltaU - 1) / deltaU;
            uint32_t numTilesV = (r.resolutionN + deltaV - 1) / deltaV;
            uint32_t totalTiles = numTilesU * numTilesV;
            uint32_t tileVerts = (deltaU + 1) * (deltaV + 1);
            uint32_t tilePrims = deltaU * deltaV * 2;

            ImGui::Text("Tile: %ux%u (%u verts, %u prims)",
                         deltaU, deltaV, tileVerts, tilePrims);
            ImGui::Text("Tiles: %ux%u = %u per element",
                         numTilesU, numTilesV, totalTiles);

            ImGui::Separator();

            // Chainmail mode
            {
                bool prev = r.chainmailMode;
                ImGui::Checkbox("Chainmail Mode", &r.chainmailMode);
                if (r.chainmailMode && !prev) {
                    r.applyPresetChainMail();
                    if (r.triangulateMesh && r.selectedMesh >= 0 &&
                        r.selectedMesh < static_cast<int>(r.assetMeshPaths.size())) {
                        r.triangulateMesh = false;
                        r.pendingMeshLoad = r.assetMeshPaths[r.selectedMesh];
                    }
                }
            }
            if (r.chainmailMode) {
                ImGui::Indent();
                ImGui::SliderFloat("Lean Amount", &r.chainmailTiltAngle, 0.0f, 1.0f, "%.2f");
                ImGui::SameLine();
                ImGui::Text("(0=flat, 1=full)");
                ImGui::Text("Metallic Shading:");
                ImGui::SliderFloat("Metal Reflectance", &r.metalF0, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Env Reflection", &r.envReflection, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Metal Diffuse", &r.metalDiffuse, 0.0f, 1.0f, "%.2f");
                ImGui::Unindent();
            }

            ImGui::Separator();
            uint32_t totalElements = r.heNbFaces + r.heNbVertices;
            ImGui::Text("Elements: %u (%u faces + %u verts)",
                         totalElements, r.heNbFaces, r.heNbVertices);
            ImGui::Text("Total mesh tasks: %u", totalElements * totalTiles);
        }

        // ==================== Pebble Controls ====================
        if (r.renderPebbles) {
            ImGui::Separator();
            auto& ubo = r.pebbleUBO;

            if (r.aoTextureLoaded)
                ImGui::Checkbox("Use AO Texture", &r.useAOTexture);
            if (r.maskTextureLoaded) {
                ImGui::Checkbox("Use Mask Texture", &r.useMaskTexture);
                if (!r.loadedMeshPath.empty()) {
                    auto dir = std::filesystem::path(r.loadedMeshPath).parent_path();
                    ImGui::TextDisabled("  %s/mask.png  (%ux%u)",
                        dir.filename().string().c_str(),
                        r.cpuMaskWidth, r.cpuMaskHeight);
                }
            }

            // Subdivision
            int subdiv = static_cast<int>(ubo.subdivisionLevel);
            if (ImGui::SliderInt("Subdivision Level", &subdiv, 0, 8)) {
                ubo.subdivisionLevel = static_cast<uint32_t>(subdiv);
                ubo.subdivOffset = std::min(ubo.subdivOffset, ubo.subdivisionLevel);
            }
            int subdivOff = static_cast<int>(ubo.subdivOffset);
            if (ImGui::SliderInt("Subdiv Offset", &subdivOff, 0,
                             std::min(static_cast<int>(ubo.subdivisionLevel), 3))) {
                ubo.subdivOffset = static_cast<uint32_t>(subdivOff);
            }

            uint32_t resolution = 1u << ubo.subdivisionLevel;
            ImGui::Text("Resolution: %ux%u (%u vertices/patch)",
                        resolution, resolution, (resolution + 1) * (resolution + 1));

            ImGui::Separator();

            // Extrusion
            ImGui::SliderFloat("Extrusion", &ubo.extrusionAmount, 0.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Variation", &ubo.extrusionVariation, 0.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Roundness", &ubo.roundness, 0.0f, 2.0f, "%.2f");

            ImGui::Separator();

            // Noise
            bool doNoise = ubo.doNoise != 0;
            ImGui::Checkbox("Noise", &doNoise);
            ubo.doNoise = doNoise ? 1 : 0;
            if (doNoise) {
                ImGui::SliderFloat("Amplitude", &ubo.noiseAmplitude, 0.0f, 1.0f,
                                   "%.3f", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Frequency", &ubo.noiseFrequency, 0.0f, 10.0f, "%.1f");
                ImGui::SliderFloat("Normal Offset", &ubo.normalOffset, 0.0f, 1.0f, "%.2f");
            }

            ImGui::Separator();

            // Culling
            bool useCulling = ubo.useCulling != 0;
            ImGui::Checkbox("Culling", &useCulling);
            ubo.useCulling = useCulling ? 1 : 0;
            if (useCulling) {
                ImGui::SameLine();
                ImGui::SliderFloat("Threshold##pebble", &ubo.cullingThreshold, 0.0f, 1.0f, "%.2f");
            }

            // LOD
            bool useLod = ubo.useLod != 0;
            ImGui::Checkbox("LOD", &useLod);
            ubo.useLod = useLod ? 1 : 0;
            if (useLod) {
                ImGui::SameLine();
                ImGui::SliderFloat("Factor##pebbleLod", &ubo.lodFactor, 0.0f, 10.0f, "%.2f");
                bool allowLow = ubo.allowLowLod != 0;
                ImGui::Checkbox("Allow Low LOD (N=0)", &allowLow);
                ubo.allowLowLod = allowLow ? 1 : 0;
            }

            ImGui::Separator();

            // Presets
            if (ImGui::Button("Smooth Pebbles")) {
                ubo.subdivisionLevel = 3;
                ubo.extrusionAmount = 0.15f;
                ubo.extrusionVariation = 0.3f;
                ubo.roundness = 2.0f;
                ubo.doNoise = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Rocky Pebbles")) {
                ubo.subdivisionLevel = 4;
                ubo.extrusionAmount = 0.2f;
                ubo.extrusionVariation = 0.5f;
                ubo.roundness = 1.0f;
                ubo.doNoise = 1;
                ubo.noiseAmplitude = 0.08f;
                ubo.noiseFrequency = 5.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cobblestone")) {
                ubo.subdivisionLevel = 2;
                ubo.extrusionAmount = 0.1f;
                ubo.extrusionVariation = 0.2f;
                ubo.roundness = 2.0f;
                ubo.doNoise = 1;
                ubo.noiseAmplitude = 0.05f;
                ubo.noiseFrequency = 8.0f;
            }

            ImGui::Separator();
            ImGui::Checkbox("Show Control Cage", &r.showControlCage);

            ImGui::Separator();
            ImGui::Text("Faces: %u", r.heNbFaces);
        }
    }

    if (r.heMeshUploaded && (r.renderResurfacing || r.renderPebbles) &&
        ImGui::CollapsingHeader("Export", ImGuiTreeNodeFlags_DefaultOpen)) {
        static char exportPath[256] = "exports/export.obj";
        ImGui::InputText("File Path", exportPath, sizeof(exportPath));

        // Estimated size
        if (r.renderResurfacing && !r.renderPebbles) {
            uint32_t M = r.resolutionM, N = r.resolutionN;
            uint32_t numElements = r.heNbFaces + r.heNbVertices;
            uint32_t estVerts = numElements * (M + 1) * (N + 1);
            uint32_t estTris = numElements * M * N * 2;
            float estMB = (estVerts * (sizeof(float) * 10) + estTris * 3 * sizeof(float) * 4) / 1e6f;
            ImGui::Text("Est: %u verts, %u tris (~%.1f MB file)", estVerts, estTris, estMB);

            if (ImGui::Button("Export Parametric Mesh")) {
                r.exportFilePath = exportPath;
                r.exportMode = 0;
                r.pendingExport = true;
            }
        }

        if (r.renderPebbles) {
            ImGui::Text("Pebble export: coming soon");
        }

        if (!r.lastExportStatus.empty()) {
            ImGui::TextWrapped("%s", r.lastExportStatus.c_str());
        }
    }

    if (r.thirdPersonMode) {
        renderPathway(r);
    }
}

void ResurfacingPanel::renderPathway(Renderer& r) {
    if (ImGui::CollapsingHeader("Pathway", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Ground Pebbles", &r.renderPathway);
        ImGui::SameLine();
        ImGui::Checkbox("Show Grid", &r.showGroundMesh);
        if (r.renderPathway) {
            ImGui::Checkbox("Fog of War", &r.fogOfWar);
            if (r.fogOfWar) {
                ImGui::Indent();
                ImGui::SliderFloat("Radius",  &r.pathwayRadius,  0.5f, 30.0f);
                ImGui::SliderFloat("Falloff", &r.pathwayFalloff, 1.0f, 30.0f);
                ImGui::Unindent();
            }
            ImGui::SliderFloat("Pebble Scale", &r.groundPebbleScale, 0.01f, 1.0f);

            ImGui::Separator();

            // Pebble resurfacing settings
            auto& ubo = r.groundPebbleUBO;

            int subdiv = static_cast<int>(ubo.subdivisionLevel);
            if (ImGui::SliderInt("Subdivision##gnd", &subdiv, 0, 8)) {
                ubo.subdivisionLevel = static_cast<uint32_t>(subdiv);
                ubo.subdivOffset = std::min(ubo.subdivOffset, ubo.subdivisionLevel);
            }
            int subdivOff = static_cast<int>(ubo.subdivOffset);
            if (ImGui::SliderInt("Subdiv Offset##gnd", &subdivOff, 0,
                             std::min(static_cast<int>(ubo.subdivisionLevel), 3))) {
                ubo.subdivOffset = static_cast<uint32_t>(subdivOff);
            }

            ImGui::SliderFloat("Extrusion##gnd", &ubo.extrusionAmount, 0.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Variation##gnd", &ubo.extrusionVariation, 0.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Roundness##gnd", &ubo.roundness, 0.0f, 2.0f, "%.2f");

            bool doNoise = ubo.doNoise != 0;
            ImGui::Checkbox("Noise##gnd", &doNoise);
            ubo.doNoise = doNoise ? 1 : 0;
            if (doNoise) {
                ImGui::SliderFloat("Amplitude##gnd", &ubo.noiseAmplitude, 0.0f, 1.0f,
                                   "%.3f", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Frequency##gnd", &ubo.noiseFrequency, 0.0f, 10.0f, "%.1f");
                ImGui::SliderFloat("Normal Offset##gnd", &ubo.normalOffset, 0.0f, 1.0f, "%.2f");
            }

            bool useCulling = ubo.useCulling != 0;
            ImGui::Checkbox("Culling##gnd", &useCulling);
            ubo.useCulling = useCulling ? 1 : 0;
            if (useCulling) {
                ImGui::SameLine();
                ImGui::SliderFloat("Threshold##gndPebble", &ubo.cullingThreshold, 0.0f, 1.0f, "%.2f");
            }

            bool useLod = ubo.useLod != 0;
            ImGui::Checkbox("LOD##gnd", &useLod);
            ubo.useLod = useLod ? 1 : 0;
            if (useLod) {
                ImGui::SameLine();
                ImGui::SliderFloat("Factor##gndLod", &ubo.lodFactor, 0.0f, 10.0f, "%.2f");
                bool allowLow = ubo.allowLowLod != 0;
                ImGui::Checkbox("Allow Low LOD##gnd", &allowLow);
                ubo.allowLowLod = allowLow ? 1 : 0;
            }

            if (ImGui::Button("Smooth##gnd")) {
                ubo.subdivisionLevel = 3;
                ubo.extrusionAmount = 0.15f;
                ubo.extrusionVariation = 0.3f;
                ubo.roundness = 2.0f;
                ubo.doNoise = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Rocky##gnd")) {
                ubo.subdivisionLevel = 4;
                ubo.extrusionAmount = 0.2f;
                ubo.extrusionVariation = 0.5f;
                ubo.roundness = 1.0f;
                ubo.doNoise = 1;
                ubo.noiseAmplitude = 0.08f;
                ubo.noiseFrequency = 5.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cobble##gnd")) {
                ubo.subdivisionLevel = 2;
                ubo.extrusionAmount = 0.1f;
                ubo.extrusionVariation = 0.2f;
                ubo.roundness = 2.0f;
                ubo.doNoise = 1;
                ubo.noiseAmplitude = 0.05f;
                ubo.noiseFrequency = 8.0f;
            }

            ImGui::Separator();
            ImGui::Text("Ground mesh");
            ImGui::Text("Mesh Type:");
            ImGui::SameLine();
            int mt = r.groundMeshType;
            if (ImGui::RadioButton("Quads",     &mt, 0)) { r.groundMeshType = 0; r.pendingGroundRegenerate = true; }
            ImGui::SameLine();
            if (ImGui::RadioButton("Pentagons", &mt, 1)) { r.groundMeshType = 1; r.pendingGroundRegenerate = true; }
            ImGui::SliderFloat("World Size", &r.groundWorldSize,     5.0f, 200.0f, "%.0f m");
            if (ImGui::IsItemDeactivatedAfterEdit()) r.pendingGroundRegenerate = true;
            ImGui::SliderFloat("Cell Size",  &r.groundPlaneCellSize, 0.05f,   2.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) r.pendingGroundRegenerate = true;
            {
                uint32_t computedN = static_cast<uint32_t>(
                    std::ceil(r.groundWorldSize / r.groundPlaneCellSize));
                computedN = std::max(computedN, 4u);
                ImGui::Text("Grid: %ux%u (%u faces)", computedN, computedN, computedN * computedN);
            }
            if (ImGui::Button("Regenerate Ground")) r.pendingGroundRegenerate = true;
        }
    }
}
