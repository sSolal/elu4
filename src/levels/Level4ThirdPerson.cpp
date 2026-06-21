#include "levels/Level4ThirdPerson.h"

#include "Renderer.h"
#include "primitives.h"
#include "mesh_merge.h"
#include "HudWidgets.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace {
    const glm::vec3 FLOOR_A {0.30f, 0.33f, 0.42f};
    const glm::vec3 FLOOR_B {0.20f, 0.22f, 0.30f};
    const glm::vec3 WALL_C  {0.34f, 0.30f, 0.46f};
    const glm::vec3 GOAL_C  {0.18f, 0.95f, 0.35f};
    const glm::vec3 ARROW_ACTIVE {0.15f, 0.95f, 0.30f};  // the arrow to reach next: green
    const glm::vec3 ARROW_DONE   {0.50f, 0.50f, 0.55f};  // already reached: grey, lingering
    // Avatar: low-W faces warm, high-W faces cool, so its W-extent reads as a
    // colour gradient painted across the body (the lesson — it is a 4D solid).
    const glm::vec3 AV_WARM {1.00f, 0.55f, 0.20f};
    const glm::vec3 AV_COOL {0.30f, 0.60f, 1.00f};
}

Level4ThirdPerson::Level4ThirdPerson() {
    // The avatar starts at the arena centre, on the floor, facing down -W (the
    // default heading every level shares), level pitch.
    avatar_.pos   = glm::vec4(0.0f, FLOOR_TOP + AVATAR_R, 0.0f, 0.0f);
    avatar_.yaw   = Math4D::Rotor4D::identity();
    avatar_.pitch = 0.0f;
    avatar_.speed = 6.0f;   // the terrain is large; run at a brisk pace

    // The inherited physics body IS the avatar's body (cam4D_ is detached from
    // physics and merely follows). Uniform radius rests it AVATAR_R above the floor.
    body_.pos    = avatar_.pos;
    body_.radius = AVATAR_R;

    // Default controls: FPS scheme, no axis lock (free X/Z/W ground movement),
    // no head-return (turning must persist so the camera trails a stable heading).
}

Level4ThirdPerson::~Level4ThirdPerson() {
    avatarBuf_.destroy();
    floorBuf_.destroy();
    goalBuf_.destroy();
    wallXBuf_.destroy();
    wallZBuf_.destroy();
    wallWBuf_.destroy();
    shaftXBuf_.destroy();
    shaftZBuf_.destroy();
    shaftWBuf_.destroy();
    headBuf_.destroy();
}

void Level4ThirdPerson::load() {
    // --- Avatar: a unit tesseract, instanced at the avatar transform. ---
    avatarMesh_ = generateHypercube();
    avatarBuf_.init(avatarMesh_);

    // --- Floor: the X/Z/W ground cut into FLOOR_DIV^3 cubes, each a cube in the
    // ground 3-space (equal X/Z/W) and thin in Y, checkered so motion — including
    // along W — reads against the grid. One big collider cube underneath tops out
    // at FLOOR_TOP (the tiles are visual; physics stays simple). ---
    const float cell = ARENA_HALF / FLOOR_DIV;   // half-extent of one ground cube
    Object4D floorTile = generateGround(glm::vec4(cell, FLOOR_HALF_Y, cell, cell));
    std::vector<ObjectInstance> ftiles;
    for (int ix = 0; ix < FLOOR_DIV; ++ix)
        for (int iz = 0; iz < FLOOR_DIV; ++iz)
            for (int iw = 0; iw < FLOOR_DIV; ++iw) {
                glm::vec4 c((2 * ix - (FLOOR_DIV - 1)) * cell, FLOOR_TOP - FLOOR_HALF_Y,
                            (2 * iz - (FLOOR_DIV - 1)) * cell,
                            (2 * iw - (FLOOR_DIV - 1)) * cell);
                glm::vec3 col = ((ix + iz + iw) & 1) ? FLOOR_B : FLOOR_A;
                ftiles.push_back({c, Math4D::Rotor4D::identity(), col, col});
            }
    // Bake the FLOOR_DIV^3 checkerboard tiles into one mesh (the single big collider
    // below already handles footing).
    floorMesh_ = mergeInstances(floorTile, ftiles);
    floorBuf_.init(floorMesh_);
    floorInsts_ = mergedInstance();
    world_.addObject(glm::vec4(0.0f, FLOOR_TOP - ARENA_HALF, 0.0f, 0.0f), ARENA_HALF);

    // --- Perimeter fence: one thin-slab mesh per horizontal axis, at + and -. ---
    const float wy = FLOOR_TOP + WALL_HALF_Y;
    wallXMesh_ = generateBox(glm::vec4(WALL_THIN, WALL_HALF_Y, ARENA_HALF, ARENA_HALF));
    wallZMesh_ = generateBox(glm::vec4(ARENA_HALF, WALL_HALF_Y, WALL_THIN, ARENA_HALF));
    wallWMesh_ = generateBox(glm::vec4(ARENA_HALF, WALL_HALF_Y, ARENA_HALF, WALL_THIN));
    wallXBuf_.init(wallXMesh_);
    wallZBuf_.init(wallZMesh_);
    wallWBuf_.init(wallWMesh_);
    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();
    for (float s : {-1.0f, 1.0f}) {
        wallXInsts_.push_back({glm::vec4(s * ARENA_HALF, wy, 0.0f, 0.0f), I, WALL_C, WALL_C});
        wallZInsts_.push_back({glm::vec4(0.0f, wy, s * ARENA_HALF, 0.0f), I, WALL_C, WALL_C});
        wallWInsts_.push_back({glm::vec4(0.0f, wy, 0.0f, s * ARENA_HALF), I, WALL_C, WALL_C});
        // Collider cubes (uniform halfSize): inner face sits exactly at ±ARENA_HALF.
        world_.addObject(glm::vec4(s * 2.0f * ARENA_HALF, 0.0f, 0.0f, 0.0f), ARENA_HALF);
        world_.addObject(glm::vec4(0.0f, 0.0f, s * 2.0f * ARENA_HALF, 0.0f), ARENA_HALF);
        world_.addObject(glm::vec4(0.0f, 0.0f, 0.0f, s * 2.0f * ARENA_HALF), ARENA_HALF);
    }

    // --- Arrow (lollipop) meshes: one long shaft per axis it can point along, plus
    // a slightly bigger head cube. Shared across every sign; the instance positions
    // place the shaft at the sign and the head at its pointing tip. ---
    shaftXMesh_ = generateBox(glm::vec4(SHAFT_LEN, SHAFT_THIN, SHAFT_THIN, SHAFT_THIN));
    shaftZMesh_ = generateBox(glm::vec4(SHAFT_THIN, SHAFT_THIN, SHAFT_LEN, SHAFT_THIN));
    shaftWMesh_ = generateBox(glm::vec4(SHAFT_THIN, SHAFT_THIN, SHAFT_THIN, SHAFT_LEN));
    headMesh_   = generateBox(glm::vec4(HEAD_HALF, HEAD_HALF, HEAD_HALF, HEAD_HALF));
    shaftXBuf_.init(shaftXMesh_);
    shaftZBuf_.init(shaftZMesh_);
    shaftWBuf_.init(shaftWMesh_);
    headBuf_.init(headMesh_);

    // --- The waypoint trail. Start facing -W; the first sign is straight ahead,
    // and each sign's arrow points to the next: left, ana, right, ana, left (far),
    // kata -> the pad. Directions: left/right = -/+X, ana/kata = +/-Z, forward = -W
    // (the same vocabulary the look-hints use in Level 3). ---
    const float y = FLOOR_TOP + SIGN_Y;
    const glm::vec4 LEFT(-1, 0, 0, 0), RIGHT(1, 0, 0, 0),
                    ANA(0, 0, 1, 0), KATA(0, 0, -1, 0), FORWARD(0, 0, 0, -1);
    glm::vec4 p(0.0f, y, 0.0f, 0.0f);
    auto step = [&](const glm::vec4& move, float dist, const glm::vec4& arrowDir) {
        p += move * dist;                 // walk to the next sign...
        signs_.push_back({p, arrowDir});  // ...whose arrow points the way onward
    };
    step(FORWARD, 10.0f, LEFT);    // sign 0: ahead, arrow -> left
    step(LEFT,    12.0f, ANA);     // sign 1: arrow -> ana
    step(ANA,     12.0f, RIGHT);   // sign 2: arrow -> right
    step(RIGHT,   12.0f, ANA);     // sign 3: arrow -> ana
    step(ANA,     12.0f, LEFT);    // sign 4: arrow -> left (the long leg)
    step(LEFT,    28.0f, KATA);    // sign 5: arrow -> kata (to the pad)
    goalCenter_ = p + KATA * 12.0f;      // the pad, one kata step beyond the last sign
    goalCenter_.y = FLOOR_TOP;

    // --- Goal pad: a bright flat slab; visual only (you walk onto it). ---
    goalMesh_ = generateBox(glm::vec4(GOAL_HALF, 0.05f, GOAL_HALF, GOAL_HALF));
    goalBuf_.init(goalMesh_);

    loaded_ = true;
}

void Level4ThirdPerson::update(const LevelContext& ctx) {
    // The avatar acts only when you're inside the 4D view; in the outer observer
    // mode it freezes (like Dodgeball) so it doesn't drift while you inspect.
    if (ctx.insideMode) {
        // Jump: edge-triggered, only when grounded. processInput's physics step
        // integrates velY (and doesn't reset it), so set it just before.
        bool space = glfwGetKey(ctx.window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (space && !spaceWasDown_ && body_.onGround)
            body_.velY = JUMP_VEL;
        spaceWasDown_ = space;

        // Drive the avatar with the EXISTING first-person input path, reused
        // verbatim: E/A run, Q/D strafe, W/S move along W, J/O turn (XW),
        // U/L turn (ZW), I/K pitch.
        avatar_.processInput(ctx.window, ctx.dt, body_, world_, controls_);
        avatar_.pos = body_.pos;

        // Advance the trail: when the avatar gets near the current target sign,
        // it counts as reached and the next sign appears (in its arrow's direction).
        if (reachedIdx_ < (int)signs_.size()) {
            glm::vec4 d = avatar_.pos - signs_[reachedIdx_].pos;
            d.y = 0.0f;   // arrows float above the floor; match on the ground plane only
            if (glm::length(d) < REACH_R)
                ++reachedIdx_;
        }
    } else {
        // Outer observer mode: drive the 3D observer camera exactly as the shared
        // runCameraInput path does for the other levels (only the inside branch is
        // special here, where input steers the avatar instead of cam4D_).
        cam3D_.processInput(ctx.window, ctx.dt);
    }

    // --- Follow-cam target: trail behind + above, looking gently down. ---
    // The avatar's facing is its view-forward: local -W (every level looks down -W).
    // "Behind" is therefore +W. The eye sits behind (-FOLLOW_DIST * facing = +W) and
    // above (+Y). It looks toward a point CAM_AIM_HEIGHT above the avatar, not at the
    // avatar itself, so the look-down stays gentle and the avatar sits low in frame.
    glm::vec4 facing = avatar_.yaw.reverse().toMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
    glm::vec4 fh(facing.x, 0.0f, facing.z, facing.w);   // yaw is Y-free; horizontal facing
    float fl = glm::length(fh);
    if (fl > 1e-4f) fh /= fl;
    glm::vec4 targetPos = avatar_.pos - FOLLOW_DIST * fh + glm::vec4(0.0f, FOLLOW_HEIGHT, 0.0f, 0.0f);
    Math4D::Rotor4D targetYaw = avatar_.yaw;            // look along the avatar's facing (-W)
    float targetPitch = -glm::degrees(std::atan2(FOLLOW_HEIGHT - CAM_AIM_HEIGHT, FOLLOW_DIST));

    if (!camInit_) {
        // First frame: snap into place so we don't swoop in from the origin.
        cam4D_.pos = targetPos; cam4D_.yaw = targetYaw; cam4D_.pitch = targetPitch;
        camInit_ = true;
    } else {
        // Frame-rate-independent exponential easing: the eye lags the avatar, which
        // makes the avatar (sharp) and the camera (trailing) clearly distinct.
        float k = 1.0f - std::exp(-CAM_SMOOTH * ctx.dt);
        cam4D_.pos   = glm::mix(cam4D_.pos, targetPos, k);
        cam4D_.yaw   = Math4D::Rotor4D::nlerp(cam4D_.yaw, targetYaw, k);
        cam4D_.yaw.normalize();
        cam4D_.pitch += (targetPitch - cam4D_.pitch) * k;
    }

    // --- Win: avatar inside the pad's X/Z/W square and grounded, briefly held. ---
    bool onPad = std::abs(avatar_.pos.x - goalCenter_.x) < GOAL_HALF
              && std::abs(avatar_.pos.z - goalCenter_.z) < GOAL_HALF
              && std::abs(avatar_.pos.w - goalCenter_.w) < GOAL_HALF
              && body_.onGround;
    winTimer_ = onPad ? winTimer_ + ctx.dt : 0.0f;
    if (winTimer_ > WIN_DWELL)
        won_ = true;
}

void Level4ThirdPerson::render(const LevelContext& ctx) {
    const Math4D::Rotor4D ori = cam4D_.getOrientation();
    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();
    auto draw = [&](const std::vector<ObjectInstance>& insts, const Object4D& mesh, ObjectBuffer& buf) {
        ctx.renderer.drawObjects(insts, mesh, buf, cam4D_, ori, focal_, ctx.innerMVP, ctx.vis);
    };

    draw(floorInsts_, floorMesh_, floorBuf_);
    draw(wallXInsts_, wallXMesh_, wallXBuf_);
    draw(wallZInsts_, wallZMesh_, wallZBuf_);
    draw(wallWInsts_, wallWMesh_, wallWBuf_);

    // Trail signs (like Level 3's landmarks): every reached arrow lingers in grey,
    // the active one — the next to reach — is green; unreached ones stay hidden. A
    // shaft pointing along its axis (mesh chosen by that axis) with a head at the tip.
    int shown = std::min(reachedIdx_, (int)signs_.size() - 1);
    for (int i = 0; i <= shown; ++i) {
        const Sign& s = signs_[i];
        const Object4D* sMesh = &shaftWMesh_;
        ObjectBuffer*   sBuf  = &shaftWBuf_;
        if (s.dir.x != 0.0f)      { sMesh = &shaftXMesh_; sBuf = &shaftXBuf_; }
        else if (s.dir.z != 0.0f) { sMesh = &shaftZMesh_; sBuf = &shaftZBuf_; }
        glm::vec3 col = (i == reachedIdx_) ? ARROW_ACTIVE : ARROW_DONE;
        std::vector<ObjectInstance> shaft = {{s.pos, I, col, col}};
        std::vector<ObjectInstance> head  = {{s.pos + s.dir * SHAFT_LEN, I, col, col}};
        draw(shaft, *sMesh, *sBuf);
        draw(head, headMesh_, headBuf_);
    }

    // Goal pad: revealed once the last sign is the target (its arrow points at it),
    // and pulses brighter while the avatar dwells on it.
    if (reachedIdx_ >= (int)signs_.size() - 1) {
        glm::vec3 goalCol = GOAL_C;
        if (winTimer_ > 0.0f) {
            float pulse = 0.5f + 0.5f * std::sin(ctx.vis.time * 6.0f);
            goalCol = glm::mix(GOAL_C, glm::vec3(1.0f), 0.4f * pulse);
        }
        std::vector<ObjectInstance> goalInst = {
            {glm::vec4(goalCenter_.x, FLOOR_TOP + 0.05f, goalCenter_.z, goalCenter_.w),
             I, goalCol, goalCol}};
        draw(goalInst, goalMesh_, goalBuf_);
    }

    // Avatar last: oriented by its heading so it visibly turns; warm/cool W gradient.
    std::vector<ObjectInstance> avInst = {{avatar_.pos, avatar_.yaw, AV_WARM, AV_COOL}};
    draw(avInst, avatarMesh_, avatarBuf_);

    ctx.renderer.drawOuterCube(ctx.outerMVP);
}

void Level4ThirdPerson::renderHUD(const LevelContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(440, 180), ImGuiCond_Always);
    ImGui::Begin("Third Person", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("Third Person - you steer the avatar; the camera trails it.");
    if (!ctx.insideMode) {
        ImGui::TextWrapped("Press TAB to step inside the 4D view.");
    } else {
        ImGui::TextWrapped("Move: E/A run, Q/D strafe, W/S along W. Turn: J/O and U/L. Space to jump.");
        if (reachedIdx_ < (int)signs_.size())
            ImGui::TextWrapped("Reach the arrow, then follow where it points. Sign %d / %zu.",
                               reachedIdx_ + 1, signs_.size());
        else
            ImGui::TextWrapped("Last arrow pointed kata - step onto the pad.");
    }
    if (won_)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "On the pad - level complete!");
    else if (winTimer_ > 0.0f)
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "On the pad - hold it!");
    ImGui::End();

    // Shared overlays (bottom-right): the view camera's 4D facing gauge and 4D
    // position. We feed the actual follow-camera (not the avatar): its pitch is
    // fixed (no look up/down in third person), so the Y bar stays put.
    glm::vec4 fw = glm::inverse(cam4D_.getOrientation().toMatrix()) * glm::vec4(0, 0, 0, -1);
    hud::drawFacingWidget(fw);
    hud::drawCoordWidget(cam4D_.pos, "Camera");
}
