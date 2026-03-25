#include "Menu.h"
#include "imgui.h"

Menu::Menu() {}
Menu::~Menu() {}

GameState Menu::renderMainMenu(GameState current) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_Always);

    ImGui::Begin("4D Game", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::Text("4D Game");
    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 200) * 0.5f);
    if (ImGui::Button("Level 1", ImVec2(200, 40))) {
        ImGui::End();
        return GameState::LEVEL_1;
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 200) * 0.5f);
    if (ImGui::Button("Level 2", ImVec2(200, 40))) {
        ImGui::End();
        return GameState::LEVEL_2;
    }

    ImGui::End();
    return current;
}

GameState Menu::renderBackButton(GameState current) {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(120, 60), ImGuiCond_Always);

    ImGui::Begin("Back", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    if (ImGui::Button("Back", ImVec2(100, 40))) {
        ImGui::End();
        return GameState::MENU;
    }

    ImGui::End();
    return current;
}
