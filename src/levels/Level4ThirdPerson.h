#pragma once

#include <vector>
#include "Level.h"
#include "Level2.h"        // ObjectInstance
#include "Object4D.h"
#include "ObjectBuffer.h"

// Level 4 — "Third Person". Teaches controlling a 4D AVATAR from outside.
//
// The lesson is the switch from "I am the camera" to "I steer a character": the
// avatar's degrees of freedom become legible — it runs forward/back, strafes,
// moves along the hidden W axis, turns in TWO horizontal planes, and jumps.
//
// Camera reuse (the design point): the engine's Camera4D is used unchanged for
// BOTH roles. `avatar_` is a Camera4D driven by the *existing* processInput, just
// like the first-person levels; the inherited view eye `cam4D_` is no longer
// keyboard-bound — it is recomputed every frame to trail behind+above the avatar
// and look at it. Camera4D::processInput carries no static state and operates on
// a passed-in PhysicsBody, so a second instance is independently drivable with no
// engine change.
class Level4ThirdPerson : public Level {
public:
    Level4ThirdPerson();
    ~Level4ThirdPerson() override;

    const char* name() const override { return "4 - Third Person"; }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    bool checkWin() const override { return won_; }
    void renderHUD(const LevelContext& ctx) override;

private:
    // The controllable entity: a Camera4D driven by the shared FPS input path.
    Camera4D avatar_;

    Object4D     avatarMesh_;   // tesseract; W-extent painted via colorA/colorB
    ObjectBuffer avatarBuf_;

    Object4D     floorMesh_;
    ObjectBuffer floorBuf_;
    std::vector<ObjectInstance> floorInsts_;

    // A directional sign: a lollipop arrow at `pos` pointing along the axis-aligned
    // unit `dir` toward the next sign (or the pad, for the last one).
    struct Sign { glm::vec4 pos; glm::vec4 dir; };
    std::vector<Sign> signs_;
    int  reachedIdx_ = 0;       // index of the sign currently being sought

    // Arrow geometry (lollipop): a long thin shaft — one mesh per axis it can point
    // along (X/Z/W) — plus a slightly bigger head cube placed at the tip. Reused for
    // every sign; the head's offset (pos + dir*SHAFT_LEN) encodes the direction sign.
    Object4D     shaftXMesh_, shaftZMesh_, shaftWMesh_, headMesh_;
    ObjectBuffer shaftXBuf_,  shaftZBuf_,  shaftWBuf_,  headBuf_;

    Object4D     goalMesh_;     // flat pad you walk the avatar onto
    ObjectBuffer goalBuf_;

    // Perimeter walls: thin slabs (one mesh per thin axis), each instanced at + and -.
    Object4D     wallXMesh_, wallZMesh_, wallWMesh_;
    ObjectBuffer wallXBuf_,  wallZBuf_,  wallWBuf_;
    std::vector<ObjectInstance> wallXInsts_, wallZInsts_, wallWInsts_;

    glm::vec4 goalCenter_{0.0f, 0.0f, 0.0f, 0.0f};   // set from the trail in load()

    bool  won_          = false;
    bool  spaceWasDown_ = false;
    float winTimer_     = 0.0f;   // dwell on the pad before it counts
    bool  camInit_      = false;  // snap the follow-cam into place on first frame

    // --- Tunables ---
    static constexpr float ARENA_HALF   = 32.0f;  // play-area half-extent (X/Z/W) — big
    static constexpr int   FLOOR_DIV    = 8;      // ground cut into FLOOR_DIV^3 cubes
    static constexpr float FLOOR_HALF_Y = 0.3f;   // floor slab half-thickness (Y)
    static constexpr float FLOOR_TOP    = 0.0f;   // y of the standing surface
    static constexpr float WALL_HALF_Y  = 0.6f;   // perimeter fence half-height
    static constexpr float WALL_THIN    = 0.1f;   // wall half-thickness in its thin axis
    static constexpr float AVATAR_R     = 0.4f;   // avatar body radius (rests this high)
    static constexpr float JUMP_VEL     = 4.5f;   // upward impulse on jump
    // Follow-cam: high above and behind, looking only gently down (third-person feel,
    // not a 45-deg top-down) so the avatar sits low in frame. It eases toward this
    // target so the lag makes "what is the avatar vs the camera" legible.
    static constexpr float FOLLOW_DIST  = 6.0f;   // eye distance behind the avatar (W)
    static constexpr float FOLLOW_HEIGHT= 4.0f;   // eye height above the avatar (Y)
    static constexpr float CAM_AIM_HEIGHT = 2.0f; // look toward this height above the avatar
    static constexpr float CAM_SMOOTH   = 5.0f;   // follow easing rate (1/s; higher=snappier)
    static constexpr float GOAL_HALF    = 1.2f;   // pad half-extent (X/Z/W)
    static constexpr float WIN_DWELL    = 0.4f;   // s on the pad to win
    // Trail / arrow signs.
    static constexpr float SIGN_Y     = 1.5f;     // arrow float height above the floor
    static constexpr float SHAFT_LEN  = 1.6f;     // arrow shaft half-length (points this way)
    static constexpr float SHAFT_THIN = 0.12f;    // shaft cross-section half-extent
    static constexpr float HEAD_HALF  = 0.32f;    // arrowhead cube half (slightly bigger)
    static constexpr float REACH_R    = 2.0f;     // distance at which a sign counts as reached
};
