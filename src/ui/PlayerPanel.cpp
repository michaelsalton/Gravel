#include "ui/PlayerPanel.h"
#include "renderer/renderer.h"
#include "imgui.h"

void PlayerPanel::render(Renderer& r) {
    if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool prevMode = r.thirdPersonMode;
        ImGui::Checkbox("Third Person Mode", &r.thirdPersonMode);
        if (r.thirdPersonMode != prevMode) {
            r.activeCamera = r.thirdPersonMode
                ? static_cast<CameraBase*>(&r.orbitCamera)
                : static_cast<CameraBase*>(&r.freeFlyCamera);
        }
        if (r.thirdPersonMode) {
            ImGui::SliderFloat("Move Speed", &r.player.speed, 0.5f, 10.0f, "%.1f");
            ImGui::SliderFloat("Sprint Multiplier", &r.player.sprintMultiplier, 1.0f, 5.0f, "%.1f");
            ImGui::SliderFloat("Camera Distance", &r.orbitCamera.distance, 1.5f, 20.0f, "%.1f");
            ImGui::SliderFloat("Rotation Smoothing", &r.player.rotationSmoothing, 1.0f, 30.0f, "%.1f");
            ImGui::Text("Position: (%.1f, %.1f, %.1f)",
                        r.player.position.x, r.player.position.y, r.player.position.z);
            const char* stateNames[] = { "Idle", "Walking", "Running" };
            int stateIdx = static_cast<int>(r.player.getAnimState());
            ImGui::Text("State: %s", stateNames[stateIdx]);
        }
    }
}
