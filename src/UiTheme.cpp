#include "UiTheme.h"
#include "imgui.h"
#include <cmath>
#include <vector>

namespace elua {

// ---- Palette -------------------------------------------------------------
ImVec4 rgb(int r, int g, int b, float a) {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

ImVec4 colBg()         { return rgb(7, 10, 18); }
ImVec4 colNight800()   { return rgb(11, 16, 25); }
ImVec4 colNight700()   { return rgb(15, 22, 35); }
ImVec4 colPanel()      { return rgb(20, 30, 44, 0.72f); }
ImVec4 colLamp()       { return rgb(227, 168, 87); }
ImVec4 colLampSoft()   { return rgb(240, 201, 135); }
ImVec4 colLampBright() { return rgb(246, 215, 159); }
ImVec4 colDrift()      { return rgb(143, 185, 168); }
ImVec4 colCeramic()    { return rgb(231, 221, 202); }
ImVec4 colCeramicHi()  { return rgb(243, 236, 222); }
ImVec4 colMuted()      { return rgb(154, 166, 173); }
ImVec4 colHairline()   { return rgb(36, 50, 70); }

// ---- Style ---------------------------------------------------------------
void applyEluaStyle() {
    ImGui::StyleColorsDark();                 // sane baseline, then recolour
    ImGuiStyle& s = ImGui::GetStyle();

    // Soft, frosted, lamplit. Rounded panels and roomy padding.
    s.WindowRounding    = 14.0f;
    s.ChildRounding     = 12.0f;
    s.FrameRounding     = 9.0f;
    s.PopupRounding     = 12.0f;
    s.GrabRounding      = 9.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 1.0f;
    s.WindowPadding     = ImVec2(20, 18);
    s.FramePadding      = ImVec2(14, 9);
    s.ItemSpacing       = ImVec2(10, 9);
    s.ItemInnerSpacing  = ImVec2(8, 6);
    s.ScrollbarRounding = 9.0f;
    s.ScrollbarSize     = 12.0f;

    ImVec4* c = s.Colors;
    const ImVec4 lamp   = colLamp();
    const ImVec4 drift  = colDrift();

    c[ImGuiCol_Text]            = colCeramic();
    c[ImGuiCol_TextDisabled]    = colMuted();
    c[ImGuiCol_WindowBg]        = colPanel();
    c[ImGuiCol_ChildBg]         = rgb(13, 19, 30, 0.0f);     // transparent by default
    c[ImGuiCol_PopupBg]         = rgb(11, 16, 25, 0.97f);
    c[ImGuiCol_Border]          = colHairline();
    c[ImGuiCol_BorderShadow]    = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]         = rgb(22, 32, 47, 0.62f);
    c[ImGuiCol_FrameBgHovered]  = rgb(30, 43, 61, 0.85f);
    c[ImGuiCol_FrameBgActive]   = rgb(36, 50, 70, 0.95f);

    c[ImGuiCol_TitleBg]         = colNight800();
    c[ImGuiCol_TitleBgActive]   = colNight700();

    // Buttons: dark glass at rest, warming toward lamp on hover/press.
    c[ImGuiCol_Button]          = rgb(26, 37, 53, 0.80f);
    c[ImGuiCol_ButtonHovered]   = rgb(227, 168, 87, 0.22f);
    c[ImGuiCol_ButtonActive]    = rgb(227, 168, 87, 0.38f);

    c[ImGuiCol_Header]          = rgb(227, 168, 87, 0.20f);  // nav/selectable highlight
    c[ImGuiCol_HeaderHovered]   = rgb(227, 168, 87, 0.28f);
    c[ImGuiCol_HeaderActive]    = rgb(227, 168, 87, 0.40f);

    c[ImGuiCol_Separator]       = colHairline();
    c[ImGuiCol_SeparatorHovered]= lamp;
    c[ImGuiCol_SeparatorActive] = lamp;

    c[ImGuiCol_CheckMark]       = lamp;
    c[ImGuiCol_SliderGrab]      = lamp;
    c[ImGuiCol_SliderGrabActive]= colLampSoft();

    c[ImGuiCol_ScrollbarBg]     = rgb(11, 16, 25, 0.0f);
    c[ImGuiCol_ScrollbarGrab]   = rgb(49, 66, 86, 0.8f);
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(143, 185, 168, 0.7f);
    c[ImGuiCol_ScrollbarGrabActive]  = drift;

    c[ImGuiCol_NavHighlight]    = lamp;
}

// ---- Fonts ---------------------------------------------------------------
static ImFont* g_body     = nullptr;
static ImFont* g_heading  = nullptr;
static ImFont* g_wordmark = nullptr;

void loadEluaFonts() {
    ImGuiIO& io = ImGui::GetIO();
    // Public Sans is the default UI font (added first → io.FontDefault).
    g_body     = io.Fonts->AddFontFromFileTTF("assets/fonts/PublicSans-400.ttf", 19.0f);
    g_heading  = io.Fonts->AddFontFromFileTTF("assets/fonts/Fraunces-600.ttf", 30.0f);
    g_wordmark = io.Fonts->AddFontFromFileTTF("assets/fonts/Fraunces-600.ttf", 104.0f);
    if (g_body) io.FontDefault = g_body;
    io.Fonts->Build();   // harmless if the backend rebuilds later
}

ImFont* fontBody()     { return g_body; }
ImFont* fontHeading()  { return g_heading; }
ImFont* fontWordmark() { return g_wordmark; }

// ---- Menu atmosphere -----------------------------------------------------
namespace {
const float TAU = 6.28318530718f;

// One braided drift strand: a slow sine, lightly wobbled by a second harmonic,
// drawn as an anti-aliased polyline across the full width.
void driftStrand(ImDrawList* dl, const ImVec2& size, float t,
                 float yFrac, float amp, float freq, float speed,
                 ImU32 col, float thickness) {
    const int N = 96;
    std::vector<ImVec2> pts;
    pts.reserve(N + 1);
    const float baseY = size.y * yFrac;
    for (int i = 0; i <= N; ++i) {
        float u = (float)i / (float)N;
        float x = u * size.x;
        float phase = t * speed;
        // Two planes turning at once (the drift conceit): a primary swing plus a
        // slower braid that makes neighbouring strands weave past each other.
        float y = baseY
                + amp * std::sin(u * freq * TAU + phase)
                + amp * 0.45f * std::sin(u * freq * 0.5f * TAU - phase * 0.7f);
        pts.push_back(ImVec2(x, y));
    }
    dl->AddPolyline(pts.data(), (int)pts.size(), col, 0, thickness);
}
} // namespace

void drawMenuAtmosphere(ImDrawList* dl, const ImVec2& size, float t) {
    // Braided currents — a few staggered strands in lamp + drift, low alpha so
    // they read as ambient motion behind the title, not foreground lines.
    const ImU32 lamp  = ImGui::GetColorU32(rgb(227, 168, 87, 0.16f));
    const ImU32 lampF = ImGui::GetColorU32(rgb(240, 201, 135, 0.10f));
    const ImU32 teal  = ImGui::GetColorU32(rgb(143, 185, 168, 0.14f));
    const ImU32 tealF = ImGui::GetColorU32(rgb(143, 185, 168, 0.08f));

    driftStrand(dl, size, t, 0.62f, 26.0f, 1.6f,  0.22f, lamp,  2.0f);
    driftStrand(dl, size, t, 0.70f, 34.0f, 1.2f, -0.16f, teal,  1.6f);
    driftStrand(dl, size, t, 0.78f, 22.0f, 2.1f,  0.30f, lampF, 1.4f);
    driftStrand(dl, size, t, 0.86f, 40.0f, 0.9f,  0.13f, tealF, 1.8f);
    driftStrand(dl, size, t, 0.55f, 18.0f, 2.6f, -0.24f, tealF, 1.2f);

    // Gentle edge vignette so the panels and footer stay legible over the glow.
    const float vh = size.y * 0.30f;
    const ImU32 dark = ImGui::GetColorU32(rgb(4, 6, 12, 0.55f));
    const ImU32 clear = ImGui::GetColorU32(rgb(4, 6, 12, 0.0f));
    dl->AddRectFilledMultiColor(ImVec2(0, size.y - vh), size, clear, clear, dark, dark);
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(size.x, vh * 0.6f), dark, dark, clear, clear);
}

void drawLogoMark(ImDrawList* dl, const ImVec2& center, float radius, float t) {
    // Breathe ~11s, the website's lamp cadence.
    float breathe = 1.0f + 0.05f * std::sin(t * TAU / 11.0f);
    float r = radius * breathe;

    // Soft amber halo behind the mark (layered translucent discs ≈ a glow).
    for (int i = 6; i >= 1; --i) {
        float rr = r * (1.0f + 0.55f * i / 6.0f);
        float a = 0.05f * (1.0f - (float)i / 7.0f) + 0.012f;
        dl->AddCircleFilled(center, rr, ImGui::GetColorU32(rgb(227, 168, 87, a)), 64);
    }

    // Outer lamp ring, inner drift ring (the website mark), slowly counter-rotating
    // tick accents to give the rings life without spinning a plain circle.
    dl->AddCircle(center, r,         ImGui::GetColorU32(rgb(227, 168, 87, 0.85f)), 96, 2.2f);
    dl->AddCircle(center, r * 0.62f, ImGui::GetColorU32(rgb(143, 185, 168, 0.70f)), 80, 1.8f);

    auto tick = [&](float ringR, float ang, ImU32 col, float len) {
        ImVec2 a(center.x + std::cos(ang) * (ringR - len), center.y + std::sin(ang) * (ringR - len));
        ImVec2 b(center.x + std::cos(ang) * (ringR + len), center.y + std::sin(ang) * (ringR + len));
        dl->AddLine(a, b, col, 2.0f);
    };
    for (int i = 0; i < 4; ++i) {
        float ang = t * 0.18f + i * (TAU / 4.0f);
        tick(r, ang, ImGui::GetColorU32(rgb(240, 201, 135, 0.6f)), 4.0f);
        tick(r * 0.62f, -t * 0.26f + i * (TAU / 4.0f) + 0.4f,
             ImGui::GetColorU32(rgb(143, 185, 168, 0.55f)), 3.0f);
    }

    // Glowing core dot.
    float coreA = 0.7f + 0.3f * std::sin(t * TAU / 5.5f);
    dl->AddCircleFilled(center, r * 0.16f, ImGui::GetColorU32(rgb(246, 215, 159, 0.25f)), 32);
    dl->AddCircleFilled(center, r * 0.10f, ImGui::GetColorU32(rgb(246, 215, 159, coreA)), 32);
}

} // namespace elua
