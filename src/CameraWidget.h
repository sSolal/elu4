#pragma once

#include <algorithm>

#include <glm/glm.hpp>

#include "imgui.h"
#include "UiTheme.h"

// The orbit camera-angle widget for the visualizer, styled like the game's in-level
// "Facing" gauge (HudWidgets.h): four axis bars (X red, Y green, Z orange, W blue)
// showing the direction the camera looks FROM — which, since it always looks at the
// object, also gives its position. A "−" and "+" button flank each axis: pressing one
// snaps all the weight onto that axis, so you jump to looking straight from ±X/Y/Z/W.
//
// `offsetDir` is the unit 4D direction from the object to the camera. Returns the
// clicked face index (0:+X 1:-X 2:+Y 3:-Y 4:+Z 5:-Z 6:+W 7:-W), or -1 if none.

namespace camwidget {

inline int draw(const glm::vec4& offsetDir, float /*t*/) {
    const char* labels[4] = {"X", "Y", "Z", "W"};
    const ImU32 colors[4] = {                       // match the game's facing widget
        IM_COL32(237, 128, 128, 255),               // X red
        IM_COL32(140, 209, 140, 255),               // Y green
        IM_COL32(242, 184, 107, 255),               // Z orange
        IM_COL32(128, 158, 237, 255),               // W blue
    };

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float winW = 260.0f;
    ImGui::SetNextWindowPos(ImVec2(disp.x - winW - 10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(winW, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("View direction", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::TextDisabled("Look from  (camera offset)");
    ImGui::Spacing();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float btnW = 30.0f, rowH = 24.0f, trackH = 12.0f, gap = 6.0f, labelW = 16.0f;
    int clicked = -1;

    for (int a = 0; a < 4; ++a) {
        ImGui::PushID(a);

        // Axis label, then the "−" button at a fixed column so the bars line up.
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colors[a]), "%s", labels[a]);
        ImGui::SameLine(labelW + 8.0f);
        if (ImGui::Button("-", ImVec2(btnW, rowH))) clicked = 2 * a + 1;   // -axis face
        ImGui::SameLine(0.0f, gap);

        // Track bar fills the space up to the "+" button at the right edge.
        float trackW = ImGui::GetContentRegionAvail().x - btnW - gap;
        if (trackW < 20.0f) trackW = 20.0f;
        ImVec2 tp = ImGui::GetCursorScreenPos();
        float ty = tp.y + (rowH - trackH) * 0.5f;
        float x0 = tp.x, x1 = tp.x + trackW, cx = (x0 + x1) * 0.5f;
        dl->AddRectFilled(ImVec2(x0, ty), ImVec2(x1, ty + trackH),
                          ImGui::GetColorU32(elua::colNight700()), 3.0f);
        dl->AddLine(ImVec2(cx, ty - 1.0f), ImVec2(cx, ty + trackH + 1.0f),
                    ImGui::GetColorU32(elua::colHairline()));
        float v = glm::clamp(offsetDir[a], -1.0f, 1.0f);
        float ex = cx + v * (trackW * 0.5f);
        dl->AddRectFilled(ImVec2(std::min(cx, ex), ty),
                          ImVec2(std::max(cx, ex), ty + trackH), colors[a], 3.0f);
        ImGui::Dummy(ImVec2(trackW, rowH));
        ImGui::SameLine(0.0f, gap);

        if (ImGui::Button("+", ImVec2(btnW, rowH))) clicked = 2 * a;       // +axis face
        ImGui::PopID();
    }

    ImGui::End();
    return clicked;
}

}  // namespace camwidget
