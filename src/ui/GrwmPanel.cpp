#include "ui/GrwmPanel.h"
#include "renderer/renderer.h"
#include "imgui.h"

void GrwmPanel::render(Renderer& r) {
    // Pipeline execution section — always visible
    if (!r.preprocessLoaded) {
        if (r.loadedMeshPath.empty()) {
            ImGui::TextDisabled("No mesh loaded");
        } else if (r.grwmRunning) {
            ImGui::TextDisabled("Running GRWM pipeline...");
        } else {
            // Pipeline settings
            ImGui::SliderInt("Slots/Face", &r.grwmSlotsPerFace, 16, 128);
            ImGui::SliderFloat("Feature Threshold", &r.grwmFeatureThreshold, 5.0f, 90.0f, "%.0f deg");

            if (ImGui::Button("Run Pipeline", ImVec2(-1, 0))) {
                r.runGrwmPreprocess();
            }
        }

        if (!r.grwmStatus.empty()) {
            ImGui::TextWrapped("%s", r.grwmStatus.c_str());
        }
        return;
    }

    // Loaded — show controls
    ImGui::Checkbox("Enable", &r.enablePreprocess);

    ImGui::SameLine();
    if (ImGui::SmallButton("Rerun")) {
        r.runGrwmPreprocess();
    }

    if (!r.grwmStatus.empty()) {
        ImGui::TextDisabled("%s", r.grwmStatus.c_str());
    }

    if (!r.enablePreprocess) return;

    ImGui::SliderFloat("Intensity", &r.grwmIntensity, 0.0f, 1.0f, "%.2f");

    ImGui::Separator();

    // Curvature density
    ImGui::Checkbox("Curvature Density", &r.enableCurvatureDensity);
    if (r.enableCurvatureDensity) {
        ImGui::Indent();
        ImGui::SliderFloat("Boost", &r.preprocessCurvatureBoost, 0.0f, 3.0f, "%.2f");
        ImGui::TextDisabled("0 = uniform  |  >1 = more on curves");
        ImGui::Unindent();
    }

    ImGui::Separator();

    // Feature edge enforcement
    ImGui::Checkbox("Feature Edge Enforcement", &r.enableFeatureEdges);
    if (r.enableFeatureEdges) {
        ImGui::Indent();
        ImGui::SliderFloat("Feature Boost", &r.featureEdgeBoost, 1.0f, 3.0f, "%.2f");
        ImGui::TextDisabled("1 = no change  |  >1 = larger on creases");
        ImGui::Unindent();
    }

    ImGui::Separator();

    // Slot placement
    ImGui::Checkbox("Slot Placement", &r.enableSlotPlacement);
    if (r.enableSlotPlacement) {
        ImGui::Indent();
        ImGui::SliderInt("Active Slots", &r.activeSlotCount, 1, 64);
        ImGui::TextDisabled("1 = center only  |  64 = max density");
        ImGui::Unindent();
    }
}
