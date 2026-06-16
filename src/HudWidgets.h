#pragma once

#include "imgui.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// Small reusable ImGui overlays shared by levels. Kept header-only (inline) so any
// level can drop them in without a new translation unit.
namespace hud {

// Bottom-right "Facing" widget: four bars showing the projection of a forward
// direction onto each global axis (X red, Y green, Z orange, W blue). The four
// squared bar lengths sum to 1 (forward is a unit vector), so it reads as a live
// 4D-heading gauge. `forward` is the world-space forward direction (unit length).
// Originally written for Level 3 (Turn & Face); factored out so other levels reuse
// it verbatim.
inline void drawFacingWidget(const glm::vec4& forward) {
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float vals[4] = { forward.x, forward.y, forward.z, forward.w };
    const char* labels[4] = { "X", "Y", "Z", "W" };
    const ImU32 colors[4] = {                  // pastel: X red, Y green, Z orange, W blue
        IM_COL32(237, 128, 128, 255),
        IM_COL32(140, 209, 140, 255),
        IM_COL32(242, 184, 107, 255),
        IM_COL32(128, 158, 237, 255),
    };
    const ImVec2 wsize(210.0f, 150.0f);
    ImGui::SetNextWindowPos(ImVec2(disp.x - wsize.x - 10.0f, disp.y - wsize.y - 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(wsize, ImGuiCond_Always);
    ImGui::Begin("Facing", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    ImVec2 wp = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(ImVec2(wp.x + 12.0f, wp.y + 8.0f), IM_COL32(225, 225, 230, 255), "Facing");
    const float pad = 12.0f, labelW = 20.0f, rowH = 28.0f, trackH = 12.0f;
    const float x0 = wp.x + pad + labelW, x1 = wp.x + wsize.x - pad, cx = (x0 + x1) * 0.5f;
    const float top = wp.y + 32.0f;
    for (int i = 0; i < 4; ++i) {
        float cy = top + i * rowH + rowH * 0.5f;
        dl->AddText(ImVec2(wp.x + pad, cy - 7.0f), IM_COL32(205, 205, 210, 255), labels[i]);
        dl->AddRectFilled(ImVec2(x0, cy - trackH * 0.5f), ImVec2(x1, cy + trackH * 0.5f),
                          IM_COL32(38, 40, 48, 200), 3.0f);
        dl->AddLine(ImVec2(cx, cy - trackH * 0.5f - 1.0f), ImVec2(cx, cy + trackH * 0.5f + 1.0f),
                    IM_COL32(120, 120, 132, 220));
        float v = glm::clamp(vals[i], -1.0f, 1.0f);
        float ex = cx + v * (x1 - x0) * 0.5f;
        dl->AddRectFilled(ImVec2(std::min(cx, ex), cy - trackH * 0.5f),
                          ImVec2(std::max(cx, ex), cy + trackH * 0.5f), colors[i], 3.0f);
    }
    ImGui::End();
}

// A 4D coordinate readout, stacked just above the facing widget (bottom-right).
// Components are rounded to the nearest unit. `label` distinguishes whose position
// it is (e.g. "Avatar" vs "Camera").
inline void drawCoordWidget(const glm::vec4& pos, const char* label = "Position") {
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float  facingH = 150.0f;             // height of drawFacingWidget, to stack above it
    const ImVec2 wsize(210.0f, 64.0f);
    ImGui::SetNextWindowPos(ImVec2(disp.x - wsize.x - 10.0f,
                                   disp.y - facingH - 10.0f - wsize.y - 8.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(wsize, ImGuiCond_Always);
    ImGui::Begin("Coord", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("%s", label);
    ImGui::Text("X %+d   Y %+d   Z %+d   W %+d",
                (int)std::lround(pos.x), (int)std::lround(pos.y),
                (int)std::lround(pos.z), (int)std::lround(pos.w));
    ImGui::End();
}

} // namespace hud
