#pragma once

// Elua's UI identity in one place: the lamplit-infrastructure palette, the ImGui
// style derived from it, the Fraunces/Public Sans fonts, and the hand-drawn menu
// atmosphere (braided drift currents + the concentric logo mark). Mirrors the
// companion website (../website/src/styles/global.css) loosely — same palette,
// fonts, mark, and slow drift motion, not a pixel match.

struct ImDrawList;
struct ImVec2;
struct ImVec4;
struct ImFont;

namespace elua {

// ---- Palette (from the website's design tokens) --------------------------
// Helpers return ImVec4 (0..1) so they can feed ImGui style/text colours; the
// IM_COL32 forms live in the .cpp where the draw-list needs packed colours.
ImVec4 rgb(int r, int g, int b, float a = 1.0f);

ImVec4 colBg();          // #070a12  the night
ImVec4 colNight800();    // #0b1019
ImVec4 colNight700();    // #0f1623
ImVec4 colPanel();       // translucent frosted panel
ImVec4 colLamp();        // #e3a857  brass/amber accent
ImVec4 colLampSoft();    // #f0c987
ImVec4 colLampBright();  // #f6d79f
ImVec4 colDrift();       // #8fb9a8  slow teal current
ImVec4 colCeramic();     // #e7ddca  default text
ImVec4 colCeramicHi();   // #f3ecde  bright text
ImVec4 colMuted();       // #9aa6ad  quiet copy
ImVec4 colHairline();    // #243246  borders

// ---- Style + fonts -------------------------------------------------------
// Apply the Elua look to the current ImGui style. Call once after the context
// exists (replaces ImGui::StyleColorsDark()).
void applyEluaStyle();

// Load Fraunces (wordmark/heading) + Public Sans (body) from assets/fonts and
// build the atlas. Safe if files are missing: the matching font pointer stays
// null and callers fall back to the default font. Call once at init.
void loadEluaFonts();

// Font handles populated by loadEluaFonts(); any may be null (use the default
// font as a fallback when so).
ImFont* fontBody();      // Public Sans, ~18px — also the ImGui default font
ImFont* fontHeading();   // Fraunces, ~30px
ImFont* fontWordmark();  // Fraunces, ~104px — the big "Elua"

// ---- Menu atmosphere (hand-drawn layer over the shader background) --------
// Braided drift currents flowing slowly across the whole viewport, plus a soft
// edge vignette. `t` is seconds (animation phase).
void drawMenuAtmosphere(ImDrawList* dl, const ImVec2& size, float t);

// The concentric-circle logo mark, breathing: outer lamp ring, inner drift
// ring, glowing core. `t` is seconds. Draw it where the menu wants it.
void drawLogoMark(ImDrawList* dl, const ImVec2& center, float radius, float t);

} // namespace elua
