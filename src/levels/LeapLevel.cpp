#include "levels/LeapLevel.h"

#include "Renderer.h"
#include "primitives.h"
#include "HudWidgets.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace {
    // Beam colours: low-W faces warm, high-W cool, so a beam's length (along W,
    // the run axis) reads as a gradient and the depth is legible as you run it.
    const glm::vec3 BEAM_A {0.42f, 0.46f, 0.60f};
    const glm::vec3 BEAM_B {0.26f, 0.30f, 0.46f};
    const glm::vec3 GOAL_C {0.18f, 0.95f, 0.35f};
    // Avatar: warm low-W / cool high-W gradient — the lesson, it is a 4D solid.
    const glm::vec3 AV_WARM {1.00f, 0.55f, 0.20f};
    const glm::vec3 AV_COOL {0.30f, 0.60f, 1.00f};
    // Sky decorations: dim, slightly cool, so they give parallax without grabbing
    // the eye away from the beams.
    const glm::vec3 DEC_A {0.50f, 0.42f, 0.60f};
    const glm::vec3 DEC_B {0.30f, 0.46f, 0.70f};
}

LeapLevel::LeapLevel() {
    // The avatar starts on the first beam, facing down -W (the shared default
    // heading) with a level pitch; it runs at a brisk pace down the course.
    avatar_.pos   = glm::vec4(0.0f, AVATAR_R, 0.0f, 0.0f);
    avatar_.yaw   = Math4D::Rotor4D::identity();
    avatar_.pitch = 0.0f;
    avatar_.speed = 6.0f;

    body_.pos    = avatar_.pos;
    body_.radius = AVATAR_R;

    // Head-return ON, springing gently back to the -W heading: turning is not part
    // of this level's lesson, so any stray turn snaps back and the course stays
    // dead ahead. Translation stays free (X/Z/W) so you can run and adjust sideways.
    controls_.headReturn         = true;
    controls_.headReturnStrength = 6.0f;
    controls_.defaultYaw         = Math4D::Rotor4D::identity();
    controls_.defaultPitch       = 0.0f;
}

LeapLevel::~LeapLevel() {
    avatarBuf_.destroy();
    goalBuf_.destroy();
    decorBuf_.destroy();
    for (auto& b : beamBufs_) b.destroy();
}

void LeapLevel::load() {
    // --- Avatar: a unit tesseract, instanced at the avatar transform. ---
    avatarMesh_ = generateHypercube();
    avatarBuf_.init(avatarMesh_);

    // --- The course: a chain of narrow beams running FORWARD along -W, at varying
    // heights and lengths with gaps between. The cursor walks forward in -W; each
    // entry records the beam's W-span (w0 = near/entry edge, w1 = far edge), its
    // top Y, and an anchor (its centre standing point) used for respawn. ---
    float cursorW = 2.0f;    // entry edge of the first beam (a little ahead of origin)
    float top     = 0.0f;    // running surface height

    auto addBeam = [&](float len, float dTop, float gap, bool goal) {
        cursorW -= gap;                 // clear the gap before this beam...
        top     += dTop;                // ...and step up/down to its height
        float w0 = cursorW;             // near (entry) edge
        float w1 = cursorW - len;       // far edge (more negative)
        Platform p;
        p.top   = top;
        p.halfX = BEAM_HALF;
        p.halfZ = BEAM_HALF;
        p.w0    = w0;
        p.w1    = w1;
        p.goal  = goal;
        p.anchor = glm::vec4(0.0f, top + AVATAR_R, 0.0f, 0.5f * (w0 + w1));
        platforms_.push_back(p);
        cursorW = w1;                   // next gap is measured from the far edge

        // Visual slab: long in W, narrow in X/Z, thin in Y; top face sits at `top`.
        float halfW = 0.5f * len;
        Object4D mesh = generateBox(glm::vec4(BEAM_HALF, FLOOR_HALF_Y, BEAM_HALF, halfW));
        beamMeshes_.push_back(std::move(mesh));

        // Colliders: a row of uniform cubes (halfSize = BEAM_HALF) tiled along the
        // span so their tops all sit at `top`. Evenly spaced from w0-BEAM_HALF to
        // w1+BEAM_HALF, enough of them to cover the beam with no gaps.
        float span = w0 - w1;
        int n = std::max(1, (int)std::ceil(span / (2.0f * BEAM_HALF)));
        for (int i = 0; i < n; ++i) {
            float t  = (n == 1) ? 0.5f : (float)i / (float)(n - 1);
            float wc = w0 - BEAM_HALF - t * (span - 2.0f * BEAM_HALF);
            world_.addObject(glm::vec4(0.0f, top - BEAM_HALF, 0.0f, wc), BEAM_HALF);
        }
    };

    //       len  dTop  gap   goal     -- mixed up-and-across; ups <= ~0.8 (a fixed
    //                                     jump clears ~1.0); longer gaps go downhill.
    addBeam(8.0f,  0.0f, 0.0f, false);  // start beam (spawn here)
    addBeam(4.0f, +0.7f, 2.5f, false);  // up & across
    addBeam(6.0f, -0.6f, 3.0f, false);  // down, longer beam (long gap)
    addBeam(3.0f, +0.6f, 2.5f, false);  // up, short beam
    addBeam(5.0f, -0.8f, 3.2f, false);  // drop, long gap
    addBeam(3.0f, +0.7f, 2.5f, false);  // up, short
    addBeam(4.0f, -0.5f, 3.0f, false);  // down/flat
    addBeam(6.0f, +0.5f, 2.5f, true);   // goal beam

    // Beam GPU buffers (1:1 with meshes/platforms).
    beamBufs_.resize(beamMeshes_.size());
    for (size_t i = 0; i < beamMeshes_.size(); ++i)
        beamBufs_[i].init(beamMeshes_[i]);

    // Goal pad overlay: a bright thin slab laid on the goal beam's top; it pulses
    // while you dwell on it (visual only — you win by standing on the beam).
    const Platform& g = platforms_.back();
    goalCenter_ = glm::vec4(0.0f, g.top + 0.06f, 0.0f, 0.5f * (g.w0 + g.w1));
    goalMesh_ = generateBox(glm::vec4(BEAM_HALF, 0.05f, BEAM_HALF, 0.5f * (g.w0 - g.w1)));
    goalBuf_.init(goalMesh_);

    // --- Sky decorations: floating tesseracts scattered around the course for
    // motion parallax (so running forward along W reads even over the void). They
    // are bigger-than-unit cubes, placed off to the sides / above the beams, and
    // never collide. Deterministic scatter via a tiny LCG so the layout is stable. ---
    decorMesh_ = generateBox(glm::vec4(0.6f, 0.6f, 0.6f, 0.6f));
    decorBuf_.init(decorMesh_);
    const float wMax = 4.0f;
    const float wMin = platforms_.back().w1 - 4.0f;
    unsigned seed = 1337u;
    auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return (float)((seed >> 8) & 0xFFFFFF) / (float)0x1000000;  // [0,1)
    };
    const int DECOR_N = 32;
    for (int i = 0; i < DECOR_N; ++i) {
        float w = wMax + (wMin - wMax) * rnd();           // spread along the course
        float side = (rnd() < 0.5f) ? -1.0f : 1.0f;
        float x = side * (4.0f + 9.0f * rnd());           // off to the sides of the beams
        float z = (rnd() - 0.5f) * 24.0f;
        float y = -2.0f + 11.0f * rnd();                  // some low, mostly above
        // A fixed varied tilt so each cube shows some 4D structure (XW / YZ planes).
        Math4D::Rotor4D ori = Math4D::Rotor4D::fromXW(rnd() * 6.28318f)
                            * Math4D::Rotor4D::fromYZ(rnd() * 6.28318f);
        decorInsts_.push_back({glm::vec4(x, y, z, w), ori, DEC_A, DEC_B});
    }

    // First checkpoint = the start beam's anchor.
    checkpoint_    = platforms_.front().anchor;
    checkpointIdx_ = 0;

    loaded_ = true;
}

void LeapLevel::update(const LevelContext& ctx) {
    if (ctx.insideMode) {
        // Jump: edge-triggered fixed impulse, only when grounded (Level 4's scheme).
        bool space = glfwGetKey(ctx.window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (space && !spaceWasDown_ && body_.onGround)
            body_.velY = JUMP_VEL;
        spaceWasDown_ = space;

        // Drive the avatar with the shared FPS input path: W/S run along -W/+W
        // (forward/back down the course), E/A and Q/D nudge sideways (X/Z).
        avatar_.processInput(ctx.window, ctx.dt, body_, world_, controls_);
        avatar_.pos = body_.pos;

        // --- Checkpoint: while grounded and within a beam's footprint, remember it
        // as the place to respawn. Updates as you progress (and harmlessly re-sets
        // to an earlier beam if you land back on one). ---
        if (body_.onGround) {
            for (int i = 0; i < (int)platforms_.size(); ++i) {
                const Platform& p = platforms_[i];
                bool inside = std::abs(avatar_.pos.x) < p.halfX + AVATAR_R
                           && std::abs(avatar_.pos.z) < p.halfZ + AVATAR_R
                           && avatar_.pos.w < p.w0 + AVATAR_R
                           && avatar_.pos.w > p.w1 - AVATAR_R
                           && std::abs(avatar_.pos.y - (p.top + AVATAR_R)) < 0.5f;
                if (inside) {
                    checkpoint_    = p.anchor;
                    checkpointIdx_ = i;
                    onGoal_        = p.goal;
                    break;
                }
            }
        }

        // --- Kill-plane: fell off → respawn at the last safe beam, killing fall
        // velocity and snapping the follow-cam so it doesn't swoop across the level. ---
        if (avatar_.pos.y < KILL_Y) {
            avatar_.pos = checkpoint_;
            body_.pos   = checkpoint_;
            body_.velY  = 0.0f;
            winTimer_   = 0.0f;
            onGoal_     = false;
            camInit_    = false;
        }
    } else {
        cam3D_.processInput(ctx.window, ctx.dt);
    }

    // --- Follow-cam target (same as Level 4): trail behind (+W) + above, looking
    // gently down toward a point above the avatar so it sits low in frame. ---
    glm::vec4 facing = avatar_.yaw.reverse().toMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
    glm::vec4 fh(facing.x, 0.0f, facing.z, facing.w);
    float fl = glm::length(fh);
    if (fl > 1e-4f) fh /= fl;
    glm::vec4 targetPos = avatar_.pos - FOLLOW_DIST * fh + glm::vec4(0.0f, FOLLOW_HEIGHT, 0.0f, 0.0f);
    Math4D::Rotor4D targetYaw = avatar_.yaw;
    float targetPitch = -glm::degrees(std::atan2(FOLLOW_HEIGHT - CAM_AIM_HEIGHT, FOLLOW_DIST));

    if (!camInit_) {
        cam4D_.pos = targetPos; cam4D_.yaw = targetYaw; cam4D_.pitch = targetPitch;
        camInit_ = true;
    } else {
        float k = 1.0f - std::exp(-CAM_SMOOTH * ctx.dt);
        cam4D_.pos   = glm::mix(cam4D_.pos, targetPos, k);
        cam4D_.yaw   = Math4D::Rotor4D::nlerp(cam4D_.yaw, targetYaw, k);
        cam4D_.yaw.normalize();
        cam4D_.pitch += (targetPitch - cam4D_.pitch) * k;
    }

    // --- Win: grounded on the goal beam, held briefly. ---
    bool onGoalNow = onGoal_ && body_.onGround;
    winTimer_ = onGoalNow ? winTimer_ + ctx.dt : 0.0f;
    if (winTimer_ > WIN_DWELL)
        won_ = true;
}

void LeapLevel::render(const LevelContext& ctx) {
    const Math4D::Rotor4D ori = cam4D_.getOrientation();
    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();
    auto draw = [&](const std::vector<ObjectInstance>& insts, const Object4D& mesh, ObjectBuffer& buf) {
        ctx.renderer.drawObjects(insts, mesh, buf, cam4D_, ori, focal_, ctx.innerMVP, ctx.vis);
    };

    // Sky decorations first (farthest context), then beams, then goal/avatar.
    draw(decorInsts_, decorMesh_, decorBuf_);

    for (size_t i = 0; i < platforms_.size(); ++i) {
        const Platform& p = platforms_[i];
        glm::vec3 a = (i & 1) ? BEAM_B : BEAM_A;
        glm::vec3 b = (i & 1) ? BEAM_A : BEAM_B;
        glm::vec4 c(0.0f, p.top - FLOOR_HALF_Y, 0.0f, 0.5f * (p.w0 + p.w1));
        std::vector<ObjectInstance> inst = {{c, I, a, b}};
        draw(inst, beamMeshes_[i], beamBufs_[i]);
    }

    // Goal pad overlay: bright green, pulsing while you dwell on it.
    glm::vec3 goalCol = GOAL_C;
    if (winTimer_ > 0.0f) {
        float pulse = 0.5f + 0.5f * std::sin(ctx.vis.time * 6.0f);
        goalCol = glm::mix(GOAL_C, glm::vec3(1.0f), 0.4f * pulse);
    }
    std::vector<ObjectInstance> goalInst = {{goalCenter_, I, goalCol, goalCol}};
    draw(goalInst, goalMesh_, goalBuf_);

    // Avatar last: oriented by its heading; warm/cool W gradient.
    std::vector<ObjectInstance> avInst = {{avatar_.pos, avatar_.yaw, AV_WARM, AV_COOL}};
    draw(avInst, avatarMesh_, avatarBuf_);

    ctx.renderer.drawOuterCube(ctx.outerMVP);
}

void LeapLevel::renderHUD(const LevelContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(440, 180), ImGuiCond_Always);
    ImGui::Begin("Leap", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("Leap - jump the avatar forward from beam to beam. Up (Y) is special.");
    if (!ctx.insideMode) {
        ImGui::TextWrapped("Press TAB to step inside the 4D view.");
    } else {
        ImGui::TextWrapped("W run forward, E/A and Q/D nudge sideways, Space to jump.");
        ImGui::TextWrapped("Stay on the beams - drift off the side or miss a jump and you fall.");
        ImGui::TextWrapped("Beam %d / %zu.", checkpointIdx_ + 1, platforms_.size());
    }
    if (won_)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "On the goal - level complete!");
    else if (winTimer_ > 0.0f)
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "On the goal - hold it!");
    ImGui::End();

    glm::vec4 fw = glm::inverse(cam4D_.getOrientation().toMatrix()) * glm::vec4(0, 0, 0, -1);
    hud::drawFacingWidget(fw);
    hud::drawCoordWidget(cam4D_.pos, "Camera");
}
