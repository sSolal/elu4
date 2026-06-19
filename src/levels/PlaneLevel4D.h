#pragma once

#include <vector>
#include "Level.h"
#include "Level2.h"        // ObjectInstance
#include "Object4D.h"
#include "ObjectBuffer.h"
#include "Math4D.h"

// Level 7 — "Plane (4D course)". The sequel Level 6 promised: genuine 4D flight.
//
// Same flight principle as Level 6 (PlaneLevel): continuous forward thrust along the
// nose (local -W) and the red cubic-Bezier guide curving from the nose to the next
// gate. Two things change, both lifting the level out of Level 6's "3D-like" cage:
//
//   1. Pitch is UNLOCKED. The third turn — Y-W (I/K) — joins X-W (J/O) and Z-W (U/L),
//      and Y is no longer pinned. The three W-plane turns span all of SO(4), so the
//      nose can point any 4D direction: "use every direction."
//   2. The gates are TILTED into 4D. Each hoop faces the direction the ideal line
//      passes through it (its `orient` rotor), so you must rotate to line up your
//      approach with each gate's facing — not merely position yourself in front of it.
//
// The course is laid out by "flying" it with the engine's own factory rotors (see the
// .cpp), so every gate's orientation is exactly a reachable craft heading and the whole
// thing is built from trusted machinery — no engine change, no hand-rolled rotors.
class PlaneLevel4D : public Level {
public:
    PlaneLevel4D();
    ~PlaneLevel4D() override;

    const char* name() const override { return "7 - Plane (4D course)"; }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    bool checkWin() const override { return won_; }
    void renderHUD(const LevelContext& ctx) override;

private:
    // The craft: full 4D position (Y free) and an unrestricted heading rotor.
    glm::vec4       pos_{0.0f};
    glm::vec4       prevPos_{0.0f};   // last frame's position, for gate-crossing tests
    Math4D::Rotor4D heading_ = Math4D::Rotor4D::identity();

    Object4D     planeMesh_;   // tesseract; warm/cool W gradient paints its W-extent
    ObjectBuffer planeBuf_;

    // Hoops: a hoop is a HOLLOW CUBE (six face panels), thin in W, but now TILTED by
    // `orient` — the craft's heading where the ideal line threads it. You pass through
    // along the gate's local -W (its nose), not the world -W. Miss one and bank around.
    struct Hoop { glm::vec4 center; Math4D::Rotor4D orient; };
    std::vector<Hoop> hoops_;
    int  currentHoop_ = 0;     // index of the next hoop to fly through

    // Three face meshes — one per axis the panel is thin along — reused for every hoop.
    Object4D     faceXMesh_, faceYMesh_, faceZMesh_;
    ObjectBuffer faceXBuf_,  faceYBuf_,  faceZBuf_;

    // Sky decorations: floating hypercubes for motion parallax.
    Object4D     decorMesh_;
    ObjectBuffer decorBuf_;
    std::vector<ObjectInstance> decorInsts_;

    // Trajectory guide: a polyline curving from the nose to the next gate's centre,
    // arriving aligned with that gate's facing. Vertices rewritten every frame.
    Object4D     guideMesh_;
    ObjectBuffer guideBuf_;
    std::vector<glm::vec4> buildGuidePoints() const;  // empty when no hoop remains

    bool  won_     = false;
    bool  camInit_ = false;    // snap the follow-cam into place on the first frame

    // --- Tunables (shared with Level 6 unless noted) ---
    static constexpr float PLANE_SPEED   = 6.0f;   // constant forward thrust
    static constexpr float TURN_SPEED    = 50.0f;  // deg/s for all three turns
    static constexpr float HOOP_OPENING  = 2.5f;   // cube half-size (clear interior to aim for)
    static constexpr float FACE_T        = 0.14f;  // face-panel half-thickness (its thin axis)
    static constexpr float FACE_W        = 0.18f;  // face-panel half-depth in W (a thin gate)
    static constexpr float PLANE_HALF_W  = 2.6f;   // plane half-length along W (long, dart-like)
    static constexpr float FOLLOW_DIST   = 9.5f;   // eye distance behind the plane
    static constexpr float FOLLOW_HEIGHT = 3.0f;   // eye height above the plane
    static constexpr float CAM_AIM_HEIGHT= 1.0f;   // look toward this height above the plane
    static constexpr float CAM_SMOOTH    = 5.0f;   // follow easing rate (1/s)

    // --- Guide line tunables ---
    static constexpr int   GUIDE_SEGMENTS = 16;    // polyline sample count (segments)
    static constexpr float GUIDE_HANDLE   = 0.55f; // Bezier tangent len as frac of chord
    static constexpr float GUIDE_NOSE_OUT = 2.9f;  // start at the nose tip (long plane)
    static constexpr glm::vec3 GUIDE_COLOR{1.0f, 0.12f, 0.10f}; // red — easy to see
};
