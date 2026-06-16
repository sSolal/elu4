#pragma once

#include <random>
#include <string>
#include <vector>
#include "Level.h"
#include "Level2.h"        // ObjectInstance
#include "Object4D.h"
#include "ObjectBuffer.h"

// Level 3 — "Turn & Face". Teaches retained heading (4D rotation) in isolation:
//   * head-return is OFF — the head stays exactly where you point it, the whole
//     lesson of this level;
//   * all translation is locked, so the player is pinned at the origin and the
//     only thing that does anything is turning;
//   * 15 landmarks are revealed one at a time at varied distances and W-offsets
//     (some tiny, some huge). Bring the active one into the forward cone — it
//     highlights — and press Space to mark it. Face all 15 to win.
//
// Guidance is staged so the large 4D look-space stays navigable: after ~10s of
// hunting, a luminous dot appears on the inner-cube border pointing the way; and
// on marking a landmark the HUD names the direction (and key) to the next one.
class TurnAndFaceLevel : public Level {
public:
    TurnAndFaceLevel();
    ~TurnAndFaceLevel() override;

    const char* name() const override { return "3 - Turn & Face"; }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    bool checkWin() const override { return won_; }
    void renderHUD(const LevelContext& ctx) override;

private:
    struct Landmark {
        glm::vec4 worldPos;   // position around the (fixed) player's eye
        bool      faced = false;
    };

    // True when `lm` sits inside the forward (-W) cone of the current camera.
    bool isFaced(const Landmark& lm) const;
    // "on your left (J)" etc. — direction from the current facing to `lm`.
    std::string hintFor(const Landmark& lm) const;

    Object4D     beaconMesh_;   // one shared hypersphere, instanced per landmark
    ObjectBuffer beaconBuf_;
    std::vector<Landmark> marks_;

    Object4D     groundMesh_;   // a flat slab the player stands on (thin in Y)
    ObjectBuffer groundBuf_;
    std::vector<ObjectInstance> groundInsts_;

    // The ground sits well below the eye so there's room to read landmarks above
    // it. Like Dodgeball: the visible floor is a thin Y-slab spanning X/Z/W, the
    // collider is a cube whose top is the standing surface, and body_.radius =
    // EYE_HEIGHT makes gravity settle the eye that far above it.
    static constexpr float EYE_HEIGHT    = 3.0f;   // eye height above the floor
    static constexpr float GROUND_XZW    = 5.0f;   // floor half-extent in X/Z/W
    static constexpr float GROUND_Y      = 0.3f;   // floor slab half-thickness (Y)
    static constexpr float FLOOR_TOP     = 0.0f;   // y of the surface you stand on

    std::mt19937 rng_{20260616u};  // deterministic layout

    int    activeIdx_   = 0;       // index into marks_ of the live landmark
    float  activeTimer_ = 0.0f;    // seconds the active landmark has been unfaced
    float  idleTimer_   = 0.0f;    // seconds since the last look-key press
    bool   pointing_    = false;   // facing the active landmark this frame
    bool   spaceWasDown_= false;
    bool   won_         = false;
    std::string nextHint_;         // HUD line shown this frame (static then live)
    std::string staticHint_;       // one-shot direction, fixed for the first 10 s

    static constexpr int   NUM_LANDMARKS = 15;
    static constexpr float CONE_DEG      = 9.0f;   // forward cone half-angle
    static constexpr float HINT_DELAY    = 10.0f;  // s before the border dot shows
    static constexpr float DIST_MIN      = 4.0f;
    static constexpr float DIST_MAX      = 22.0f;

    // Pitch (only) eases back to level after a short idle, very slowly — yaw is
    // retained (the lesson), but a drifting horizon is disorienting.
    static constexpr float PITCH_RETURN_DELAY = 2.0f;   // s of no looking before it starts
    static constexpr float PITCH_RETURN_RATE  = 0.6f;   // 1/s decay (gentle)
};
