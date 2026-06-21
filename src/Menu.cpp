#include "Menu.h"
#include "LevelRegistry.h"
#include "UiTheme.h"
#include "imgui.h"
#include <cfloat>

Menu::Menu() {}
Menu::~Menu() {}

namespace {

// Centered draw-list text in a chosen font/size, with graceful fallback to the
// default font when a vendored font failed to load. Optionally draws a soft
// shadow first for legibility over the glow.
void centerText(ImDrawList* dl, ImFont* font, float size, const char* s,
                float cx, float y, ImU32 col, bool shadow = false) {
    if (!font) { font = ImGui::GetFont(); size = ImGui::GetFontSize(); }
    ImVec2 sz = font->CalcTextSizeA(size, FLT_MAX, 0.0f, s);
    ImVec2 p(cx - sz.x * 0.5f, y);
    if (shadow) {
        ImU32 sh = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.55f));
        dl->AddText(font, size, ImVec2(p.x + 2, p.y + 3), sh, s);
    }
    dl->AddText(font, size, p, col, s);
}

// Center the next item of the given width within a full-width window.
void centerNextItem(float winW, float itemW) {
    ImGui::SetCursorPosX((winW - itemW) * 0.5f);
}

} // namespace

float Menu::drawTitle(float winW, float winH, float t) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cx = winW * 0.5f;

    // Logo mark, then the wordmark, then the two taglines — stacked and centered.
    // The whole block stays compact on short windows so the panel below fits.
    float markCy = winH * 0.14f;
    float markR  = winH * 0.058f;
    if (markR < 32.0f) markR = 32.0f;
    elua::drawLogoMark(dl, ImVec2(cx, markCy), markR, t);

    float wordSize = elua::fontWordmark() ? 104.0f : 64.0f;
    // Scale the wordmark down on short windows so the title never crowds out
    // the level panel / credits card.
    if (winH < 680.0f) wordSize *= winH / 680.0f;
    float wordY = markCy + markR * 1.35f;
    centerText(dl, elua::fontWordmark(), wordSize, "Elua", cx, wordY,
               ImGui::GetColorU32(elua::colLampBright()), true);

    float afterWord = wordY + wordSize * 0.96f;

    // Eyebrow tagline (small-caps feel via uppercase) + a softer serif line.
    centerText(dl, elua::fontBody(), 16.0f, "K E E P   T H E   L A M P S   L I T",
               cx, afterWord, ImGui::GetColorU32(elua::colLamp()));
    centerText(dl, elua::fontHeading(), 22.0f, "An open world held by many hands",
               cx, afterWord + 25.0f, ImGui::GetColorU32(elua::colCeramic()));

    return afterWord + 54.0f;   // window-local y where content begins
}

int Menu::drawLevelPanel(float winW, float winH, float contentTop) {
    const auto& levels = levelRegistry();
    int selected = -1;

    const float listW = 384.0f;
    // Fill the space between the title and the Credits button + footer so the
    // panel never spills past the window bottom (it scrolls internally instead).
    float listH = winH - contentTop - 92.0f;
    if (listH > 430.0f) listH = 430.0f;
    if (listH < 170.0f) listH = 170.0f;

    ImGui::SetCursorPos(ImVec2((winW - listW) * 0.5f, contentTop));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, elua::colPanel());
    ImGui::BeginChild("levellist", ImVec2(listW, listH), true);

    ImGui::PushStyleColor(ImGuiCol_Text, elua::colMuted());
    ImGui::TextUnformatted("WALK A STRETCH");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));

    for (int i = 0; i < (int)levels.size(); ++i) {
        ImGui::PushID(i);
        const LevelEntry& e = levels[i];

        // Grey out unbuilt levels (still clickable - they load a placeholder).
        if (!e.implemented)
            ImGui::PushStyleColor(ImGuiCol_Text, elua::colMuted());

        if (ImGui::Button(e.name.c_str(), ImVec2(-1, 38)))
            selected = i;

        // Initial focus for keyboard/gamepad nav (used in VR).
        if (i == 0)
            ImGui::SetItemDefaultFocus();

        if (!e.implemented)
            ImGui::PopStyleColor();

        ImGui::PopID();
        ImGui::Dummy(ImVec2(0, 4));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Credits button, centered under the panel.
    const float btnW = 168.0f;
    ImGui::SetCursorPos(ImVec2((winW - btnW) * 0.5f, contentTop + listH + 16.0f));
    if (ImGui::Button("Credits", ImVec2(btnW, 36.0f)))
        showCredits = true;

    // Footer line, pinned near the bottom edge.
    centerText(ImGui::GetWindowDrawList(), elua::fontBody(), 14.0f,
               "Elua  ·  a 4D game  ·  MIT © 2026 Solal Stern",
               winW * 0.5f, winH - 30.0f, ImGui::GetColorU32(elua::colMuted()));

    return selected;
}

void Menu::drawCredits(float winW, float winH, float contentTop) {
    const float cardW = 540.0f;
    // Use the height left under the title so the Back button stays on-screen.
    float cardH = winH - contentTop - 22.0f;
    if (cardH > 440.0f) cardH = 440.0f;
    if (cardH < 240.0f) cardH = 240.0f;

    ImGui::SetCursorPos(ImVec2((winW - cardW) * 0.5f, contentTop));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, elua::colPanel());
    ImGui::BeginChild("credits", ImVec2(cardW, cardH), true);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 5));  // tighter rows so it fits at 600p

    if (elua::fontHeading()) ImGui::PushFont(elua::fontHeading());
    ImGui::PushStyleColor(ImGuiCol_Text, elua::colLampBright());
    ImGui::TextUnformatted("Elua");
    ImGui::PopStyleColor();
    if (elua::fontHeading()) ImGui::PopFont();

    ImGui::PushStyleColor(ImGuiCol_Text, elua::colCeramic());
    ImGui::TextWrapped("A 4D game about the quiet dignity of keeping a vast, "
                       "half-understood machine alive — held by many hands.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 4));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));

    auto row = [](const char* k, const char* v) {
        ImGui::PushStyleColor(ImGuiCol_Text, elua::colMuted());
        ImGui::TextUnformatted(k);
        ImGui::PopStyleColor();
        ImGui::SameLine(150.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, elua::colCeramic());
        ImGui::TextUnformatted(v);
        ImGui::PopStyleColor();
    };
    row("Created by",  "Solal Stern");
    row("Built with",  "Dear ImGui · GLFW · GLEW · GLM · OpenGL");
    row("Fonts",       "Fraunces & Public Sans (OFL)");
    row("License",     "MIT © 2026 Solal Stern");

    ImGui::Dummy(ImVec2(0, 8));
    if (ImGui::Button("Back", ImVec2(120.0f, 34.0f)))
        showCredits = false;

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

int Menu::renderMainMenu() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 size = vp->Size;
    const float  t    = (float)ImGui::GetTime();

    // Hand-drawn drift currents + vignette behind everything (over the shader).
    elua::drawMenuAtmosphere(ImGui::GetBackgroundDrawList(), size, t);

    ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));   // see the shader through it
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##eluamenu", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                 ImGuiWindowFlags_NoScrollbar);

    float contentTop = drawTitle(size.x, size.y, t);

    int selected = -1;
    if (showCredits)
        drawCredits(size.x, size.y, contentTop);
    else
        selected = drawLevelPanel(size.x, size.y, contentTop);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    return selected;
}

// The Back/Settings windows are just invisible anchors for a single button —
// transparent and borderless so only the button's own panel shows (no frosted
// window panel or border doubling up behind it).
static bool floatingButton(const char* id, const char* label, const ImVec2& pos) {
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(116, 48), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);
    bool clicked = ImGui::Button(label, ImVec2(116, 44));
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    return clicked;
}

bool Menu::renderBackButton() {
    return floatingButton("Back", "Back", ImVec2(10, 10));
}

bool Menu::renderSettingsButton() {
    // Stacked just below the Back button; kept clear of the level HUD at (140,10).
    return floatingButton("SettingsBtn", "Settings", ImVec2(10, 64));
}
