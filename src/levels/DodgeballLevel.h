#pragma once

#include <random>
#include <vector>
#include "Level.h"
#include "Level2.h"        // ObjectInstance
#include "Object4D.h"
#include "ObjectBuffer.h"

// Level 2 — "Dodgeball". Teaches the +/-X/+/-Z strafe plane in isolation:
//   * strafing is fully unlocked (the whole point), W-motion still works but is
//     useless for dodging;
//   * head-return keeps the player facing straight down -W, the one direction
//     they can perceive motion from. So balls (hyperspheres) approach head-on
//     along +W, each at a random X/Z offset drawn from a Gaussian centred on the
//     player, reading as growing dots aimed roughly at you;
//   * survive WIN_SECONDS of escalating waves by side-stepping in X/Z. Get hit
//     and the wave soft-resets (balls clear, timer restarts) so you retry.
class DodgeballLevel : public Level {
public:
    DodgeballLevel();
    ~DodgeballLevel() override;

    const char* name() const override { return "2 - Dodgeball"; }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    bool checkWin() const override { return won_; }
    void renderHUD(const LevelContext& ctx) override;

private:
    struct Projectile {
        glm::vec4 pos;
        glm::vec4 vel;     // vel.y is live (arc), vel.w is the constant approach speed
        float     gravY;   // downward accel applied to vel.y, sized so the arc lands at eye height
        float     radius;
        glm::vec3 color;
    };

    void spawnBall();          // launch one ball aimed near the player
    void onHit();              // soft reset of the current run

    // Floor: a checkerboard of thin box tiles filling the (X,Z,W) ground volume.
    Object4D     floorMesh_;
    ObjectBuffer floorBuf_;
    std::vector<ObjectInstance> floorInsts_;

    // Balls: one shared hypersphere mesh, instanced per live projectile each frame.
    Object4D     ballMesh_;
    ObjectBuffer ballBuf_;
    std::vector<Projectile> balls_;

    std::mt19937 rng_{1234u};  // deterministic seed -> repeatable waves for debugging

    float survival_  = 0.0f;   // seconds survived this run, counts up to WIN_SECONDS
    float spawnTimer_= 0.0f;   // time since last spawn
    float nextSpawn_ = 1.0f;   // randomised gap until the next spawn (Poisson timing)
    float hitFlash_  = 0.0f;   // >0 while the "Hit!" message shows after a reset
    bool  won_       = false;

    static constexpr float WIN_SECONDS = 45.0f;
    static constexpr float W_SPAWN     = -40.0f;  // balls appear this far down -W
    static constexpr float W_DESPAWN   =   3.0f;  // culled once past the player by this

    // The player is a vertical capsule from the floor (feet) up to the camera
    // (head), so a ball connects if it lands anywhere on the body, not just the eye.
    static constexpr float EYE_HEIGHT  =   1.7f;  // camera/head height above the floor
    static constexpr float PLAYER_R    =   0.5f;  // body half-width (horizontal hit radius)

    // Each ball still goes up then down, but its peak and the height at which it
    // reaches the player are randomised: some plunge to the feet, some stay high
    // and strike the head.
    static constexpr float ARC_PEAK_MIN = 1.5f;   // min apex above the launch height
    static constexpr float ARC_PEAK_MAX = 5.0f;   // max apex above the launch height
};
