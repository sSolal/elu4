#include "Menu.h"
#include "LevelRegistry.h"
#include "imgui.h"

Menu::Menu() {}
Menu::~Menu() {}

int Menu::renderMainMenu() {
    const auto& levels = levelRegistry();

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(340, 460), ImGuiCond_Always);

    ImGui::Begin("4D Game", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::Text("4D Game - Select a level");
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    int selected = -1;

    // Scrollable list so all ten levels fit regardless of count.
    ImGui::BeginChild("levellist", ImVec2(0, 380), false);
    for (int i = 0; i < (int)levels.size(); ++i) {
        ImGui::PushID(i);
        const LevelEntry& e = levels[i];

        // Grey out unbuilt levels (still clickable - they load a placeholder).
        if (!e.implemented)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

        if (ImGui::Button(e.name.c_str(), ImVec2(-1, 36)))
            selected = i;

        if (!e.implemented)
            ImGui::PopStyleColor();

        ImGui::PopID();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
    }
    ImGui::EndChild();

    ImGui::End();
    return selected;
}

bool Menu::renderBackButton() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(120, 60), ImGuiCond_Always);

    ImGui::Begin("Back", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    bool clicked = ImGui::Button("Back", ImVec2(100, 40));

    ImGui::End();
    return clicked;
}
