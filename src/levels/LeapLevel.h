#pragma once

#include <vector>
#include "Level.h"
#include "Level2.h"        // ObjectInstance
#include "Object4D.h"
#include "ObjectBuffer.h"

// Level 5 — "Leap". Teaches that the Y axis is special (up / gravity).
//
// A straight platforming course running FORWARD along -W (the axis the third-
// person camera looks down, so the W key advances into the course). The avatar
// runs along a chain of narrow "beams" at varying heights, jumping the gaps
// between them. Drift sideways (X/Z) off a beam and you fall; mistime a jump and
// you fall. Falling past the kill-plane respawns you at the last beam you landed
// on. Reach and dwell on the goal beam to win.
//
// Reuses the Level 4 third-person scaffold verbatim: the avatar is a Camera4D
// driven by the shared FPS input path, the inherited cam4D_ is recomputed each
// frame to trail behind+above it, and the jump is a fixed velY impulse. The only
// genuinely new logic is multi-height beam colliders and the kill-plane/respawn.
class LeapLevel : public Level {
public:
    LeapLevel();
    ~LeapLevel() override;

    const char* name() const override { return "5 - Leap"; }

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

    // One beam in the course. The visual slab is long in W (the run axis), narrow
    // in X/Z, thin in Y. Physics is a row of uniform collider cubes tiled along W
    // (the engine's colliders are uniform-halfSize hypercubes), whose tops sit at
    // `top`. `anchor` is the respawn standing point (beam centre X/Z, top+AVATAR_R,
    // entry-W).
    struct Platform {
        glm::vec4 anchor;       // respawn / checkpoint standing point
        float     top;          // y of the standing surface
        float     halfX, halfZ; // sideways half-extents (narrow)
        float     w0, w1;       // W span of the beam (w1 < w0; runs toward -W)
        bool      goal;         // the final beam
    };
    std::vector<Platform> platforms_;

    // ObjectInstance carries no scale, so each beam gets its own mesh sized to its
    // span. Stored 1:1 with platforms_.
    std::vector<Object4D>     beamMeshes_;
    std::vector<ObjectBuffer> beamBufs_;

    Object4D     goalMesh_;     // overlay slab drawn on the goal beam (pulses)
    ObjectBuffer goalBuf_;

    // Sky decorations: floating hypercubes for motion parallax. Visual only.
    Object4D     decorMesh_;
    ObjectBuffer decorBuf_;
    std::vector<ObjectInstance> decorInsts_;

    // --- Runtime state ---
    glm::vec4 checkpoint_{0.0f};   // last safe standing point (set on landing)
    int       checkpointIdx_ = 0;  // which platform that was
    bool      onGoal_        = false;
    bool      won_           = false;
    bool      spaceWasDown_  = false;
    float     winTimer_      = 0.0f;   // dwell on the goal beam before it counts
    bool      camInit_       = false;  // snap the follow-cam on first frame / respawn

    glm::vec4 goalCenter_{0.0f};   // centre of the goal beam (set in load())

    // --- Tunables ---
    static constexpr float FLOOR_HALF_Y = 0.3f;   // beam slab half-thickness (Y)
    static constexpr float BEAM_HALF    = 0.9f;   // sideways half-width (X & Z) of a beam
    static constexpr float AVATAR_R     = 0.4f;   // avatar body radius (rests this high)
    static constexpr float JUMP_VEL     = 4.5f;   // upward impulse on jump (fixed height)
    static constexpr float KILL_Y       = -12.0f; // fall below this -> respawn
    // Follow-cam (same feel as Level 4): high above and behind, looking gently down.
    static constexpr float FOLLOW_DIST  = 6.0f;
    static constexpr float FOLLOW_HEIGHT= 4.0f;
    static constexpr float CAM_AIM_HEIGHT = 2.0f;
    static constexpr float CAM_SMOOTH   = 5.0f;
    static constexpr float WIN_DWELL    = 0.4f;   // s on the goal beam to win
};
