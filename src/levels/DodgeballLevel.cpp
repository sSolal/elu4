#include "levels/DodgeballLevel.h"

#include "Renderer.h"
#include "primitives.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

// The floor is a 4D ground volume: in 4D the "ground" the player walks on is the
// 3-space (X,Z,W) at a fixed height Y, so we tile it with a checkerboard of cube
// blocks across all three of those axes. The player stands on top (Y), faces down
// -W, and dodges in the X/Z plane.
namespace {
    constexpr float TILE   = 2.0f;          // floor block half-extent in X/Z/W
    constexpr float STEP   = 2.0f * TILE;   // block spacing (blocks abut)
    constexpr float TOP_Y  = 0.0f;          // top surface the player stands on
    constexpr float TILE_HALF_Y = 0.5f;     // floor blocks are thin slabs in Y

    constexpr int   NX = 2;                 // X tiles run -NX..NX  -> 5 blocks
    constexpr int   NZ = 2;                 // Z tiles run -NZ..NZ  -> 5 blocks
    constexpr float W_NEAR = 0.0f;          // ground spans W from far end up to here
    constexpr float W_FAR  = -28.0f;
}

DodgeballLevel::DodgeballLevel() {
    // Stand on the ground at the origin, looking straight down -W. The collision
    // radius doubles as the standing height: gravity settles the eye EYE_HEIGHT
    // above the floor, giving the player a tall body for balls to strike.
    cam4D_.pos   = glm::vec4(0.0f, EYE_HEIGHT, 0.0f, 0.0f);
    cam4D_.speed = 3.0f;     // snappier than the 1.5 default so dodging feels responsive
    body_.radius = EYE_HEIGHT;

    // The mechanic this level teaches: free X/Z strafing, with the head springing
    // back to face the incoming balls down -W.
    controls_.lockedAxes         = AxisLock::NONE;   // strafing is the whole point
    controls_.lockIsWorldSpace   = true;
    controls_.headReturn         = true;             // keep facing the incoming balls
    controls_.headReturnStrength = 7.0f;
    controls_.defaultYaw         = Math4D::Rotor4D::identity();
    controls_.defaultPitch       = 0.0f;
}

DodgeballLevel::~DodgeballLevel() {
    floorBuf_.destroy();
    ballBuf_.destroy();
}

void DodgeballLevel::load() {
    // Shared meshes: one thin floor slab, one ball.
    floorMesh_ = generateBox({TILE, TILE_HALF_Y, TILE, TILE});
    floorBuf_.init(floorMesh_);
    ballMesh_  = generateHypersphere();   // radius 0.5
    ballBuf_.init(ballMesh_);

    // Calm two-green checkerboard so the warm balls read clearly against it.
    const glm::vec3 floorShade[2] = {{0.26f, 0.36f, 0.28f}, {0.17f, 0.26f, 0.20f}};
    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();
    const float visualY = TOP_Y - TILE_HALF_Y;          // slab centre so its top sits at TOP_Y
    const float collideY = TOP_Y - TILE;                 // cube collider whose top also sits at TOP_Y

    int wi = 0;
    for (float w = W_NEAR; w >= W_FAR; w -= STEP, ++wi) {
        for (int ix = -NX; ix <= NX; ++ix) {
            for (int iz = -NZ; iz <= NZ; ++iz) {
                float x = ix * STEP, z = iz * STEP;
                int parity = (ix + iz + wi) & 1;
                glm::vec3 shade = floorShade[parity];
                floorInsts_.push_back({glm::vec4(x, visualY, z, w), I, shade, shade});
                // Continuous collision floor: cube colliders abut across X/Z/W.
                world_.addObject(glm::vec4(x, collideY, z, w), TILE);
            }
        }
    }

    loaded_ = true;
}

void DodgeballLevel::spawnBall() {
    // Aim tightly at the player: a small Gaussian offset in X/Z so that, standing
    // still, most balls (~3/4) would connect — moving is what saves you. The spread
    // tightens further as the run goes on.
    float t     = std::min(survival_ / WIN_SECONDS, 1.0f);
    float sigma = 0.65f - 0.15f * t;
    float speed = 8.0f + 8.0f * t;          // approach speed along +W, ramps up
    std::normal_distribution<float> off(0.0f, sigma);

    glm::vec4 p = cam4D_.pos;
    p.x += off(rng_);
    p.z += off(rng_);
    p.w  = W_SPAWN;

    // Parabolic arc in Y, randomised per ball. The ball launches from eye height,
    // rises to a random apex A above it, and reaches a random height yLand on the
    // player's body (feet..head) at the instant it crosses the player's W-plane.
    // Solving y(Thit) = yLand and apex = A for the launch speed/gravity gives
    //   vY0 = (2/Thit)[A + sqrt(A*(A-D))],  g = vY0^2 / (2A),   D = yLand - eye.
    // The lowest point of the arc before the crossing is yLand itself, so a ball
    // never dips to the floor before reaching the player (no early ground hit).
    std::uniform_real_distribution<float> landDist(0.0f, cam4D_.pos.y);          // feet..head
    std::uniform_real_distribution<float> peakDist(ARC_PEAK_MIN, ARC_PEAK_MAX);

    float travel = cam4D_.pos.w - W_SPAWN;  // distance to cover along +W (positive)
    float thit   = travel / speed;
    float yLand  = landDist(rng_);
    float A      = peakDist(rng_);
    float D      = yLand - cam4D_.pos.y;    // net vertical change over the flight (<= 0)
    float vY0    = (2.0f / thit) * (A + std::sqrt(A * (A - D)));
    float gravY  = vY0 * vY0 / (2.0f * A);

    balls_.push_back({p, glm::vec4(0.0f, vY0, 0.0f, speed), gravY, 0.5f,
                      glm::vec3(0.95f, 0.35f, 0.20f)});
}

void DodgeballLevel::onHit() {
    balls_.clear();
    survival_   = 0.0f;
    spawnTimer_ = 0.0f;
    nextSpawn_  = 1.0f;
    hitFlash_   = 1.5f;
}

void DodgeballLevel::update(const LevelContext& ctx) {
    runCameraInput(ctx);

    if (hitFlash_ > 0.0f)
        hitFlash_ = std::max(0.0f, hitFlash_ - ctx.dt);

    // The wave only runs in the 4D FPS view; in the 3D observer mode it freezes.
    if (won_ || !ctx.insideMode)
        return;

    survival_ += ctx.dt;
    if (survival_ >= WIN_SECONDS) {
        won_ = true;
        return;
    }

    // Spawn scheduler: random (Poisson) timing so balls come in bursts and lulls,
    // with the AVERAGE gap shrinking over the run -> sparse at first, busy at the end.
    float t    = survival_ / WIN_SECONDS;
    float mean = 2.0f - 1.6f * t;           // average gap, 2.0 s -> 0.4 s
    spawnTimer_ += ctx.dt;
    if (spawnTimer_ >= nextSpawn_) {
        spawnTimer_ = 0.0f;
        spawnBall();
        std::exponential_distribution<float> gap(1.0f / mean);
        nextSpawn_ = gap(rng_);
    }

    // Advance balls (gravity arcs them down onto the player), test for a hit, then
    // cull the ones that have passed us.
    for (auto& b : balls_) {
        b.vel.y -= b.gravY * ctx.dt;
        b.pos   += b.vel * ctx.dt;
        // Player as a vertical capsule: nearest point on the feet->head segment to
        // the ball, then a sphere overlap. Hits anywhere on the body, high or low.
        glm::vec4 nearest = cam4D_.pos;
        nearest.y = std::clamp(b.pos.y, 0.0f, cam4D_.pos.y);   // floor (feet) .. eye (head)
        if (glm::length(b.pos - nearest) < b.radius + PLAYER_R) {
            onHit();
            return;   // balls_ was cleared; stop iterating
        }
    }
    balls_.erase(std::remove_if(balls_.begin(), balls_.end(),
                     [&](const Projectile& b) { return b.pos.w > cam4D_.pos.w + W_DESPAWN; }),
                 balls_.end());
}

void DodgeballLevel::render(const LevelContext& ctx) {
    Math4D::Rotor4D ori = cam4D_.getOrientation();

    ctx.renderer.drawObjects(floorInsts_, floorMesh_, floorBuf_, cam4D_, ori, focal_, ctx.innerMVP, ctx.vis);

    // One instance per live ball, drawn in a single batched call.
    std::vector<ObjectInstance> ballInsts;
    ballInsts.reserve(balls_.size());
    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();
    for (const auto& b : balls_)
        ballInsts.push_back({b.pos, I, b.color, b.color});
    if (!ballInsts.empty())
        ctx.renderer.drawObjects(ballInsts, ballMesh_, ballBuf_, cam4D_, ori, focal_, ctx.innerMVP, ctx.vis);

    ctx.renderer.drawOuterCube(ctx.outerMVP);
}

void DodgeballLevel::renderHUD(const LevelContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Always);
    ImGui::Begin("Dodgeball", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("Dodgeball - balls come straight at you down the depth axis.");
    if (!ctx.insideMode) {
        ImGui::TextWrapped("Press TAB to step inside the 4D view.");
    } else {
        ImGui::TextWrapped("Strafe the four horizontal directions to dodge. "
                           "Moving forward/back (W) is useless here.");
        if (won_)
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Level complete - you survived!");
        else
            ImGui::Text("Survive: %.0f s left", std::max(0.0f, WIN_SECONDS - survival_));
        if (hitFlash_ > 0.0f && !won_)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "Hit! Wave reset - try again.");
    }
    ImGui::End();
}
