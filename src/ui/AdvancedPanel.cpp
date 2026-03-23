#include "ui/AdvancedPanel.h"
#include "renderer/renderer.h"
#include "core/window.h"
#include "imgui.h"

void AdvancedPanel::render(Renderer& r) {
    if (ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Display
        bool prevVsync = r.vsync;
        ImGui::Checkbox("VSync", &r.vsync);
        if (r.vsync != prevVsync) {
            r.pendingSwapChainRecreation = true;
        }
        // MSAA
        const char* msaaLabels[] = { "Off", "2x", "4x", "8x" };
        int msaaOptions[] = { 1, 2, 4, 8 };
        int currentMsaa = 0;
        for (int i = 0; i < 4; i++) {
            if (msaaOptions[i] == r.msaaSampleCount) currentMsaa = i;
        }
        if (ImGui::Combo("MSAA", &currentMsaa, msaaLabels, 4)) {
            int newCount = msaaOptions[currentMsaa];
            if (newCount != r.msaaSampleCount) {
                r.msaaSampleCount = newCount;
                r.pendingMsaaChange = true;
            }
        }

        ImGui::Separator();

        // Culling
        ImGui::Checkbox("Frustum Culling", &r.enableFrustumCulling);
        ImGui::Checkbox("Back-Face Culling", &r.enableBackfaceCulling);
        if (r.enableBackfaceCulling) {
            ImGui::SliderFloat("Threshold", &r.cullingThreshold, -1.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            if (ImGui::Button("Reset##threshold")) r.cullingThreshold = 0.0f;
        }

        ImGui::Separator();

        // LOD
        ImGui::Checkbox("Adaptive LOD", &r.enableLod);
        if (r.enableLod) {
            ImGui::SliderFloat("LOD Factor", &r.lodFactor, 0.1f, 5.0f, "%.2f");
            ImGui::TextDisabled("< 1.0 = performance  |  > 1.0 = quality");
        }

        ImGui::Separator();

        // Specular AA
        ImGui::Checkbox("Specular AA", &r.enableSpecularAA);
        if (r.enableSpecularAA) {
            ImGui::SliderFloat("AA Strength", &r.specularAAStrength, 0.0f, 2.0f, "%.2f");
        }

        ImGui::Checkbox("Coverage Fade", &r.enableCoverageFade);
        if (r.enableCoverageFade) {
            ImGui::SliderFloat("Fade Start", &r.coverageFadeStart, 0.001f, 0.05f, "%.3f");
            ImGui::SliderFloat("Fade End", &r.coverageFadeEnd, 0.0001f, 0.02f, "%.4f");
        }
    }
}
