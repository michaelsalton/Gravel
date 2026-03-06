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
        if (ImGui::Button(r.getWindow().isFullscreen() ? "Windowed" : "Fullscreen")) {
            r.getWindow().toggleFullscreen();
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
    }
}
