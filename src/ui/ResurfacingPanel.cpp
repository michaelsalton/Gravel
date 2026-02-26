#include "ui/ResurfacingPanel.h"
#include "renderer/renderer.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>

#ifndef ASSETS_DIR
#define ASSETS_DIR ""
#endif

void ResurfacingPanel::render(Renderer& r) {
    if (ImGui::CollapsingHeader("Resurfacing", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* surfaceTypes[] = {"Torus", "Sphere", "Cone", "Cylinder"};
        int currentType = static_cast<int>(r.elementType);
        if (ImGui::Combo("Surface Type", &currentType, surfaceTypes, 4)) {
            r.elementType = static_cast<uint32_t>(currentType);
        }

        if (r.elementTypeTextureLoaded)
            ImGui::Checkbox("Use Element Type Map", &r.useElementTypeTexture);
        if (r.aoTextureLoaded)
            ImGui::Checkbox("Use AO Texture", &r.useAOTexture);
        if (r.maskTextureLoaded)
            ImGui::Checkbox("Use Mask Texture", &r.useMaskTexture);

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

        // Resolution controls (tiling allows beyond single-tile limit)
        ImGui::Text("UV Grid Resolution:");
        int resM = static_cast<int>(r.resolutionM);
        int resN = static_cast<int>(r.resolutionN);
        if (ImGui::SliderInt("Resolution M", &resM, 2, 64)) {
            r.resolutionM = static_cast<uint32_t>(resM);
        }
        if (ImGui::SliderInt("Resolution N", &resN, 2, 64)) {
            r.resolutionN = static_cast<uint32_t>(resN);
        }

        // Compute tile info (mirror shader getDeltaUV logic)
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
            const char* meshPaths[] = {
                ASSETS_DIR "cube.obj",
                ASSETS_DIR "plane.obj",
                ASSETS_DIR "plane5x5.obj",
                ASSETS_DIR "sphere.obj",
                ASSETS_DIR "sphere_hd.obj",
                ASSETS_DIR "icosphere.obj",
                ASSETS_DIR "dragon/dragon_8k.obj",
                ASSETS_DIR "dragon/dragon_coat.obj",
                ASSETS_DIR "boy.obj",
                ASSETS_DIR "man/man.obj"
            };
            bool prev = r.chainmailMode;
            ImGui::Checkbox("Chainmail Mode", &r.chainmailMode);
            if (r.chainmailMode && !prev) {
                r.applyPresetChainMail();
                if (r.triangulateMesh) {
                    r.triangulateMesh = false;
                    r.pendingMeshLoad = meshPaths[r.selectedMesh];
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
}
