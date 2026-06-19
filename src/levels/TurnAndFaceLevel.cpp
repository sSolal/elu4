#include "levels/TurnAndFaceLevel.h"

#include "Renderer.h"
#include "Tesseract.h"
#include "primitives.h"
#include "HudWidgets.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace {
    // Beacon state colours. Fog is disabled for this level, so they stay vivid at
    // any distance.
    const glm::vec3 COL_ACTIVE     {0.15f, 0.95f, 0.30f};  // the one to find: green
    const glm::vec3 COL_HIGHLIGHT  {1.00f, 0.55f, 0.10f};  // on target: orange
    const glm::vec3 COL_FACED      {0.25f, 0.45f, 0.95f};  // already seen: blue
}

TurnAndFaceLevel::TurnAndFaceLevel() {
    // Stand at eye height above the floor, looking straight down -W (identity yaw,
    // level pitch). radius = EYE_HEIGHT so gravity settles the eye on the floor.
    cam4D_.pos   = glm::vec4(0.0f, EYE_HEIGHT, 0.0f, 0.0f);
    cam4D_.yaw   = Math4D::Rotor4D::identity();
    cam4D_.pitch = 0.0f;
    body_.radius = EYE_HEIGHT;

    // The lesson: turning is free and PERSISTENT (no spring-back), but you can't
    // travel — every axis of translation is locked, so only looking does anything.
    controls_.lockedAxes       = AxisLock::X | AxisLock::Y | AxisLock::Z | AxisLock::W;
    controls_.lockIsWorldSpace = true;
    controls_.headReturn       = false;
}

TurnAndFaceLevel::~TurnAndFaceLevel() {
    beaconBuf_.destroy();
    groundBuf_.destroy();
}

void TurnAndFaceLevel::load() {
    beaconMesh_ = generateHypersphere();
    beaconBuf_.init(beaconMesh_);

    // The floor: a flat slab, thin in Y and extended across the X/Z/W 3-space
    // (the "ground" of a 4D world), so it reads as a plane — not a hypercube.
    // Visible slab top sits at FLOOR_TOP; a cube collider with the same top does
    // the actual standing (PhysicsAABB is uniform, so the visual and collider are
    // separate, exactly as in Dodgeball).
    groundMesh_ = generateBox(glm::vec4(GROUND_XZW, GROUND_Y, GROUND_XZW, GROUND_XZW));
    groundBuf_.init(groundMesh_);
    groundInsts_.push_back({glm::vec4(0.0f, FLOOR_TOP - GROUND_Y, 0.0f, 0.0f),
                            Math4D::Rotor4D::identity(),
                            glm::vec3(0.28f, 0.31f, 0.40f), glm::vec3(0.18f, 0.20f, 0.27f)});
    world_.addObject(glm::vec4(0.0f, FLOOR_TOP - EYE_HEIGHT, 0.0f, 0.0f), EYE_HEIGHT);

    // Beacons live in the horizontal X/Z/W 3-space around the eye (the directions
    // you reach by TURNING — the lesson), with only gentle up/down so the floor
    // never occludes them and the pitch clamp never blocks a facing. Distances
    // vary so some read as tiny far specks and some as big near orbs.
    std::normal_distribution<float>  gauss(0.0f, 1.0f);
    std::uniform_real_distribution<float> dist(DIST_MIN, DIST_MAX);
    // Keep beacons within +/-45 deg of the horizon: looking far up or down in 4D
    // is disorienting, so the task stays mostly about turning.
    std::uniform_real_distribution<float> elev(glm::radians(-45.0f), glm::radians(45.0f));
    for (int i = 0; i < NUM_LANDMARKS; ++i) {
        glm::vec4 h(gauss(rng_), 0.0f, gauss(rng_), gauss(rng_));   // horizontal: X/Z/W
        float hl = glm::length(h);
        if (hl < 1e-4f) h = glm::vec4(0, 0, 0, -1), hl = 1.0f;
        h /= hl;
        float e = elev(rng_);
        glm::vec4 d = h * std::cos(e) + glm::vec4(0, 1, 0, 0) * std::sin(e);
        Landmark lm;
        lm.worldPos = cam4D_.pos + d * dist(rng_);
        marks_.push_back(lm);
    }
    // The first beacon sits dead ahead at eye level (camera starts looking down
    // -W), so the player immediately sees one and learns to mark it.
    if (!marks_.empty())
        marks_[0].worldPos = cam4D_.pos + glm::vec4(0.0f, 0.0f, 0.0f, -9.0f);

    activeIdx_   = 0;
    activeTimer_ = 0.0f;
    nextHint_.clear();
    // First beacon is dead ahead (so this is hidden anyway), but seed it so the
    // one-shot hint is always defined.
    staticHint_  = marks_.empty() ? "" : hintFor(marks_[0]);
    loaded_      = true;
}

bool TurnAndFaceLevel::isFaced(const Landmark& lm) const {
    // World->camera transform, identical to the renderer's (camera looks down -W).
    glm::vec4 cs = cam4D_.getOrientation().toMatrix() * (lm.worldPos - cam4D_.pos);
    float len = glm::length(cs);
    if (len < 1e-4f) return true;
    float cosF = -cs.w / len;                       // 1 when dead ahead (-W)
    return cosF > std::cos(glm::radians(CONE_DEG));
}

std::string TurnAndFaceLevel::hintFor(const Landmark& lm) const {
    // Robust to rotor sign conventions: simulate each of the six look keys exactly
    // as Camera4D applies them, and name whichever turn best aims at the target
    // (largest forward-cone alignment). The named key is therefore always correct.
    glm::vec4 rel = lm.worldPos - cam4D_.pos;
    const float a = glm::radians(20.0f);
    const Math4D::Rotor4D y = cam4D_.yaw;
    const float p = cam4D_.pitch;
    struct Cand { Math4D::Rotor4D yaw; float pitch; const char* text; };
    const Cand cands[6] = {
        { Math4D::Rotor4D::fromXW(-a) * y, p, "Look left (J)" },
        { Math4D::Rotor4D::fromXW( a) * y, p, "Look right (O)" },
        { Math4D::Rotor4D::fromZW(-a) * y, p, "Look ana (U)" },
        { Math4D::Rotor4D::fromZW( a) * y, p, "Look kata (L)" },
        { y, glm::clamp(p + 20.0f, -89.0f, 89.0f), "Look above (I)" },
        { y, glm::clamp(p - 20.0f, -89.0f, 89.0f), "Look below (K)" },
    };
    float best = -2.0f;
    const char* bestText = "";
    for (const Cand& c : cands) {
        Math4D::Rotor4D ori = Math4D::Rotor4D::fromYW(glm::radians(c.pitch)) * c.yaw;
        glm::vec4 cs = ori.toMatrix() * rel;
        float len = glm::length(cs);
        float cosF = len > 1e-4f ? -cs.w / len : -1.0f;
        if (cosF > best) { best = cosF; bestText = c.text; }
    }
    return bestText;
}

void TurnAndFaceLevel::update(const LevelContext& ctx) {
    runCameraInput(ctx);   // movement is a no-op (all axes locked); only looking acts

    // This level wants beacon colours to read at any depth, so force fog off.
    ctx.vis.depth = DepthCue::Normal;

    // Slowly ease pitch back to level after a short idle (yaw stays put). Only the
    // six look keys count as "looking"; everything else is locked anyway.
    auto down = [&](int k) { return glfwGetKey(ctx.window, k) == GLFW_PRESS; };
    bool looking = down(GLFW_KEY_J) || down(GLFW_KEY_O) || down(GLFW_KEY_U) ||
                   down(GLFW_KEY_L) || down(GLFW_KEY_I) || down(GLFW_KEY_K);
    idleTimer_ = looking ? 0.0f : idleTimer_ + ctx.dt;
    if (idleTimer_ > PITCH_RETURN_DELAY)
        cam4D_.pitch *= std::exp(-PITCH_RETURN_RATE * ctx.dt);

    if (won_ || marks_.empty()) return;

    Landmark& active = marks_[activeIdx_];
    activeTimer_ += ctx.dt;
    pointing_ = ctx.insideMode && isFaced(active);

    // Two-phase guidance. For the first HINT_DELAY seconds you only get the single
    // direction shown when you bagged the previous beacon (fixed — find it yourself).
    // After that, the hint goes live (recomputed each frame, so it tracks as you
    // rotate and resolves multi-turn targets) and the visual dot appears.
    if (pointing_)                          nextHint_.clear();
    else if (activeTimer_ < HINT_DELAY)     nextHint_ = staticHint_;
    else                                    nextHint_ = hintFor(active);

    bool space = glfwGetKey(ctx.window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (pointing_ && space && !spaceWasDown_) {
        active.faced = true;
        ++activeIdx_;
        activeTimer_ = 0.0f;
        if (activeIdx_ >= (int)marks_.size())
            won_ = true;
        else
            staticHint_ = hintFor(marks_[activeIdx_]);   // one-shot, from this facing
    }
    spaceWasDown_ = space;
}

void TurnAndFaceLevel::render(const LevelContext& ctx) {
    Math4D::Rotor4D ori = cam4D_.getOrientation();
    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();

    std::vector<ObjectInstance> insts;
    // Faced beacons linger (blue) so the constellation visibly fills in.
    for (int i = 0; i < (int)marks_.size(); ++i) {
        if (i == activeIdx_ || !marks_[i].faced) continue;
        insts.push_back({marks_[i].worldPos, I, COL_FACED, COL_FACED});
    }
    // The active beacon: green normally, orange (pulsing) while you're on target.
    if (!won_ && activeIdx_ < (int)marks_.size()) {
        const Landmark& a = marks_[activeIdx_];
        glm::vec3 col = COL_ACTIVE;
        if (pointing_) {
            float pulse = 0.5f + 0.5f * std::sin(ctx.vis.time * 6.0f);
            col = glm::mix(COL_HIGHLIGHT, glm::vec3(1.0f), 0.25f * pulse);
        }
        insts.push_back({a.worldPos, I, col, col});
    }
    ctx.renderer.drawObjects(groundInsts_, groundMesh_, groundBuf_, cam4D_, ori, focal_, ctx.innerMVP, ctx.vis);
    ctx.renderer.drawObjects(insts, beaconMesh_, beaconBuf_, cam4D_, ori, focal_, ctx.innerMVP, ctx.vis);
    ctx.renderer.drawOuterCube(ctx.outerMVP);
}

bool TurnAndFaceLevel::faceDotPoint(const LevelContext& ctx, glm::vec3& outLocal) const {
    // The dot only shows once you've been hunting a while and aren't on target.
    if (!(ctx.insideMode && !won_ && activeIdx_ < (int)marks_.size()
          && activeTimer_ > HINT_DELAY && !pointing_))
        return false;
    const Landmark& a = marks_[activeIdx_];
    glm::vec4 cs = cam4D_.getOrientation().toMatrix() * (a.worldPos - cam4D_.pos);
    glm::vec3 lat(cs.x, cs.y, cs.z);
    if (glm::length(lat) <= 1e-3f) return false;
    lat = glm::normalize(lat);
    float m = std::max(std::fabs(lat.x), std::max(std::fabs(lat.y), std::fabs(lat.z)));
    outLocal = 0.48f * lat / m;   // on the inner-cube face, in the beacon's direction
    return true;
}

void TurnAndFaceLevel::renderWorldHUD(const LevelContext& ctx) {
    // VR: draw the direction dot as a real 3D marker on the cube face (projected
    // with the per-eye MVP) instead of as a flat ImGui overlay on the HUD panel.
    glm::vec3 dot;
    if (!faceDotPoint(ctx, dot)) return;
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    float aspect = vp[3] > 0 ? (float)vp[2] / (float)vp[3] : 1.0f;
    float pulse = 0.5f + 0.5f * std::sin(ctx.vis.time * 5.0f);
    ctx.renderer.drawMarker(dot, 0.045f, aspect, glm::vec3(1.0f), 0.55f + 0.45f * pulse,
                            ctx.innerMVP);
}

void TurnAndFaceLevel::renderHUD(const LevelContext& ctx) {
    const ImVec2 disp = ImGui::GetIO().DisplaySize;

    // --- Off-screen direction indicator: a flat, bright-white dot (clearly an
    // overlay, not a scene object). After hunting too long, it sits on the inner
    // cube's face in the beacon's direction. We project the cube-face point with
    // the SAME innerMVP the scene uses, so it tracks the observer camera exactly.
    // In VR this is drawn in 3D by renderWorldHUD instead (worldHUD == true).
    glm::vec3 dotLocal;
    if (!ctx.worldHUD && faceDotPoint(ctx, dotLocal)) {
        glm::vec4 clip = ctx.innerMVP * glm::vec4(dotLocal, 1.0f);  // on the cube face
        if (clip.w > 1e-4f) {
            float sx = (clip.x / clip.w * 0.5f + 0.5f) * disp.x;
            float sy = (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * disp.y;
            float pulse = 0.5f + 0.5f * std::sin(ctx.vis.time * 5.0f);
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            dl->AddCircleFilled(ImVec2(sx, sy), 12.0f, IM_COL32(255, 255, 255, (int)(80 * pulse)));
            dl->AddCircleFilled(ImVec2(sx, sy), 5.0f,  IM_COL32(255, 255, 255, 255));
        }
    }

    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(420, 170), ImGuiCond_Always);
    ImGui::Begin("Turn & Face", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("Turn & Face - your head stays where you point it.");
    if (!ctx.insideMode) {
        ImGui::TextWrapped("Press TAB to step inside the 4D view.");
    } else {
        ImGui::TextWrapped("Look: J/O left/right, U/L ana/kata, I/K up/down. You can't move - only turn.");
        ImGui::Text("Faced %d / %d", activeIdx_, NUM_LANDMARKS);
        if (!nextHint_.empty())   // hidden once the beacon is in view
            ImGui::TextWrapped("%s", nextHint_.c_str());
    }
    if (won_)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "All landmarks faced - level complete!");
    else if (pointing_)
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f), "On target - press SPACE to mark it.");
    ImGui::End();

    // --- Orientation widget (bottom-right): four bars showing the projection of
    // the current forward direction onto each global axis. Forward is a unit
    // vector, so the four squared bar lengths always sum to 1. At the start you
    // look down -W, so W reads full; turning bleeds that weight onto X/Y/Z.
    // (Shared widget, see HudWidgets.h.)
    glm::vec4 fw = glm::inverse(cam4D_.getOrientation().toMatrix()) * glm::vec4(0, 0, 0, -1);
    hud::drawFacingWidget(fw);
}
