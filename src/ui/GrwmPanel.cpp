#include "ui/GrwmPanel.h"
#include "renderer/renderer.h"
#include "imgui.h"

void GrwmPanel::render(Renderer& r) {
    if (!r.preprocessLoaded) {
        ImGui::TextDisabled("No preprocess data loaded");
        ImGui::TextDisabled("Place GRWM output in <mesh>/preprocess/");
        return;
    }

    ImGui::Checkbox("Enable", &r.enablePreprocess);

    if (!r.enablePreprocess) return;

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
        ImGui::TextDisabled("Holds resolution along sharp creases");
        ImGui::Unindent();
    }

}
