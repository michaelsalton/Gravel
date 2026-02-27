#include "ui/AdvancedPanel.h"
#include "renderer/renderer.h"
#include "imgui.h"

#include <cmath>

void AdvancedPanel::render(Renderer& r) {
    if (ImGui::CollapsingHeader("Advanced")) {
        // Display
        bool prevVsync = r.vsync;
        ImGui::Checkbox("VSync", &r.vsync);
        if (r.vsync != prevVsync) {
            r.pendingSwapChainRecreation = true;
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

        // Statistics
        {
            // Base mesh
            ImGui::Text("Base Mesh:");
            ImGui::Indent();
            ImGui::Text("Faces:     %u", r.heNbFaces);
            ImGui::Text("Vertices:  %u", r.heNbVertices);
            ImGui::Text("Triangles: %u", r.baseMeshTriCount);
            ImGui::Unindent();

            ImGui::Separator();

            // Procedural
            ImGui::Text("Procedural:");
            ImGui::Indent();
            bool doMaskCull = r.useMaskTexture && r.maskTextureLoaded && !r.cpuMaskPixels.empty();
            bool doCulling = r.enableFrustumCulling || r.enableBackfaceCulling;

            // Masked elements don't exist — they reduce the total, not the visible count
            auto isMasked = [&](glm::vec2 uv) -> bool {
                if (!doMaskCull) return false;
                uint32_t x = static_cast<uint32_t>(uv.x * r.cpuMaskWidth) % r.cpuMaskWidth;
                uint32_t y = static_cast<uint32_t>(uv.y * r.cpuMaskHeight) % r.cpuMaskHeight;
                return r.cpuMaskPixels[y * r.cpuMaskWidth + x] < 128;
            };

            uint32_t totalElements = 0;
            uint32_t visibleElements = 0;

            if (r.renderResurfacing && r.heMeshUploaded) {
                glm::mat4 mvp;
                if (doCulling) {
                    float aspect = static_cast<float>(r.swapChainExtent.width) /
                                   static_cast<float>(r.swapChainExtent.height);
                    mvp = r.activeCamera->getProjectionMatrix(aspect) * r.activeCamera->getViewMatrix();
                }

                auto testElement = [&](glm::vec3 pos, glm::vec3 normal, float area) -> bool {
                    float radius = std::sqrt(area) * r.userScaling * 2.0f;
                    if (r.enableFrustumCulling) {
                        glm::vec4 clip = mvp * glm::vec4(pos, 1.0f);
                        if (clip.w <= 0.0f) return false;
                        float cr = radius / clip.w * 2.0f * 1.1f;
                        glm::vec3 ndc = glm::vec3(clip) / clip.w;
                        if (ndc.x + cr < -1.0f || ndc.x - cr > 1.0f) return false;
                        if (ndc.y + cr < -1.0f || ndc.y - cr > 1.0f) return false;
                        if (ndc.z + cr <  0.0f || ndc.z - cr > 1.0f) return false;
                    }
                    if (r.enableBackfaceCulling) {
                        glm::vec3 viewDir = glm::normalize(r.activeCamera->getPosition() - pos);
                        if (glm::dot(viewDir, normal) <= r.cullingThreshold) return false;
                    }
                    return true;
                };

                for (uint32_t i = 0; i < r.heNbFaces; i++) {
                    if (isMasked(r.cpuFaceUVs[i])) continue;
                    totalElements++;
                    if (!doCulling || testElement(r.cpuFaceCenters[i], r.cpuFaceNormals[i], r.cpuFaceAreas[i]))
                        visibleElements++;
                }
                for (uint32_t i = 0; i < r.heNbVertices; i++) {
                    if (isMasked(r.cpuVertexUVs[i])) continue;
                    totalElements++;
                    if (!doCulling || testElement(r.cpuVertexPositions[i], r.cpuVertexNormals[i], r.cpuVertexFaceAreas[i]))
                        visibleElements++;
                }
            }

            uint32_t culledElements = totalElements - visibleElements;
            uint32_t trisPerElement = r.resolutionM * r.resolutionN * 2;
            uint32_t proceduralTris = visibleElements * trisPerElement;

            ImGui::Text("Elements:  %u visible / %u total", visibleElements, totalElements);
            if (totalElements > 0) {
                float pct = 100.0f * culledElements / totalElements;
                ImGui::Text("Culled:    %u (%.1f%%)", culledElements, pct);
            }
            ImGui::Text("Triangles: %u  (%u per element)", proceduralTris, trisPerElement);
            ImGui::Unindent();

            ImGui::Separator();

            // Total
            uint32_t totalTris = r.baseMeshTriCount + proceduralTris;
            ImGui::Text("Total Triangles: %u", totalTris);
        }
    }
}
