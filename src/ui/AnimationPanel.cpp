#include "ui/AnimationPanel.h"
#include "renderer/renderer.h"
#include "imgui.h"

void AnimationPanel::render(Renderer& r) {
    if (r.skeletonLoaded && ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Skinning", &r.doSkinning);
        if (r.thirdPersonMode) {
            ImGui::Text("Play Animation: %s (Auto)", r.animationPlaying ? "On" : "Off");
            ImGui::Text("Speed: %.2f (Auto)", r.animationSpeed);
        } else {
            ImGui::Checkbox("Play Animation", &r.animationPlaying);
            ImGui::SliderFloat("Speed", &r.animationSpeed, 0.0f, 5.0f, "%.2f");
        }

        float duration = r.animations.empty() ? 0.0f : r.animations[0].duration;
        ImGui::SliderFloat("Time", &r.animationTime, 0.0f, duration, "%.3f s");
        ImGui::SameLine();
        if (ImGui::Button("Reset##anim")) r.animationTime = 0.0f;

        ImGui::Text("Bones: %u", r.boneCount);
        if (!r.animations.empty()) {
            ImGui::Text("Animation: \"%s\" (%.2fs)", r.animations[0].name.c_str(), duration);
        }
    }
}
