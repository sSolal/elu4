#pragma once

#include <vector>
#include "Level.h"
#include "Level2.h"        // ObjectInstance
#include "Object4D.h"
#include "ObjectBuffer.h"
#include "Math4D.h"

// Level 6 — "Plane (3D-like)". Teaches flight as a familiar anchor.
//
// The craft is under continuous forward thrust along its nose (local -W, the same
// "forward" every level uses). Pitch (the Y-W plane) is LOCKED, so the nose never
// climbs or dives; the only controls are the two horizontal turns already in the
// engine — X-W (J/O) and Z-W (U/L). With Y held constant the plane flies through
// the {X,Z,W} horizontal 3-space exactly like an ordinary 3D plane with no gravity.
// Level 7 will unlock the remaining dimension for genuine 4D flight.
//
// No engine change is needed: with pitch locked the chase camera is the standard
// flat-horizon model, so cam4D_.getOrientation() and the Renderer work unchanged.
// The flight model lives entirely here (we do not route through Camera4D).
class PlaneLevel : public Level {
public:
    PlaneLevel();
    ~PlaneLevel() override;

    const char* name() const override { return "6 - Plane (3D-like)"; }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    bool checkWin() const override { return won_; }
    void renderHUD(const LevelContext& ctx) override;

private:
    // The plane: position (Y constant) and a heading rotor restricted to {X,Z,W}.
    glm::vec4       pos_{0.0f};
    Math4D::Rotor4D heading_ = Math4D::Rotor4D::identity();

    Object4D     planeMesh_;   // tesseract; warm/cool W gradient paints its W-extent
    ObjectBuffer planeBuf_;

    // Hoops: a hoop is a HOLLOW CUBE in {X,Y,Z} (six face panels: ±X, ±Y, ±Z),
    // thin in W, strung along -W. The plane threads through the cube's hollow centre
    // along the W axis — the 4D generalisation of flying through a ring. Miss one and
    // you simply bank around and re-approach (no teleport).
    struct Hoop { glm::vec4 center; };
    std::vector<Hoop> hoops_;
    int  currentHoop_ = 0;     // index of the next hoop to fly through

    // Three face meshes — one per axis the panel is thin along — reused for every
    // hoop (each hoop instances each mesh twice, at + and - of that axis).
    Object4D     faceXMesh_, faceYMesh_, faceZMesh_;
    ObjectBuffer faceXBuf_,  faceYBuf_,  faceZBuf_;

    // Sky decorations: floating hypercubes for motion parallax.
    Object4D     decorMesh_;
    ObjectBuffer decorBuf_;
    std::vector<ObjectInstance> decorInsts_;

    // Trajectory guide: a polyline curving from the nose to the next hoop's centre,
    // ending aligned with -W (the direction you must pass through). Vertices are
    // rewritten every frame from the live position/heading. Helps the pilot see, in
    // 4D, how to bank to arrive both centred and pointing the right way.
    Object4D     guideMesh_;
    ObjectBuffer guideBuf_;
    std::vector<glm::vec4> buildGuidePoints() const;  // empty when no hoop remains

    bool  won_     = false;
    bool  camInit_ = false;    // snap the follow-cam into place on the first frame

    // --- Tunables ---
    static constexpr float FLIGHT_Y      = 0.0f;   // constant flight height (pitch locked)
    static constexpr float PLANE_SPEED   = 6.0f;   // constant forward thrust
    static constexpr float TURN_SPEED    = 50.0f;  // deg/s for both horizontal turns
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
