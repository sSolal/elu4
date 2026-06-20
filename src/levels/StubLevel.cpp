#include "levels/StubLevel.h"

#include "Renderer.h"
#include "primitives.h"
#include "imgui.h"

void StubLevel::load() {
    // One slowly-recognisable marker cube so the viewport isn't empty.
    scene_.push_back({
        {0.0f, kHyperHalf, 0.0f, 0.0f},
        Math4D::Rotor4D::fromXW(0.6f) * Math4D::Rotor4D::fromXZ(0.4f),
        glm::vec3(0.5f, 0.5f, 0.55f),
        glm::vec3(0.3f, 0.3f, 0.35f)
    });
    loaded_ = true;
}

void StubLevel::update(const LevelContext& ctx) {
    runCameraInput(ctx);
}

void StubLevel::render(const LevelContext& ctx) {
    ctx.renderer.drawObjects(scene_, ctx.hyperMesh, ctx.hyperBuf, cam4D_, cam4D_.getOrientation(),
                             focal_, ctx.innerMVP, ctx.vis);
    ctx.renderer.drawOuterCube(ctx.outerMVP);
}

void StubLevel::renderHUD(const LevelContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(360, 90), ImGuiCond_Always);
    ImGui::Begin(name_.c_str(), nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("%s", name_.c_str());
    ImGui::Separator();
    ImGui::TextWrapped("Not yet built. %s", blurb_.c_str());
    ImGui::End();
}
