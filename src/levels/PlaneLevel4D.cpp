#include "levels/PlaneLevel4D.h"

#include "Renderer.h"
#include "primitives.h"
#include "HudWidgets.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace {
    // Hoop colours by state: the next one to fly through is bright green, already
    // passed ones dim, upcoming ones a neutral blue.
    const glm::vec3 HOOP_ACTIVE {0.15f, 0.95f, 0.30f};
    const glm::vec3 HOOP_PASSED {0.34f, 0.34f, 0.40f};
    const glm::vec3 HOOP_NEXT   {0.45f, 0.55f, 0.85f};
    // Plane: low-W faces warm, high-W faces cool, so its W-extent reads as a gradient.
    const glm::vec3 AV_WARM {1.00f, 0.55f, 0.20f};
    const glm::vec3 AV_COOL {0.30f, 0.60f, 1.00f};
    // Sky decorations: dim so they stay background context.
    const glm::vec3 DEC_A {0.26f, 0.28f, 0.34f};
    const glm::vec3 DEC_B {0.18f, 0.20f, 0.26f};

    const glm::vec4 LOCAL_FWD{0.0f, 0.0f, 0.0f, -1.0f};  // every craft's local "nose"
}

PlaneLevel4D::PlaneLevel4D() {
    // Start at the origin facing -W (the shared default heading).
    pos_     = glm::vec4(0.0f);
    prevPos_ = pos_;
    heading_ = Math4D::Rotor4D::identity();
}

PlaneLevel4D::~PlaneLevel4D() {
    planeBuf_.destroy();
    faceXBuf_.destroy();
    faceYBuf_.destroy();
    faceZBuf_.destroy();
    decorBuf_.destroy();
    guideBuf_.destroy();
}

void PlaneLevel4D::load() {
    // --- Plane: a box stretched long along W so it reads as a slender dart. ---
    planeMesh_ = generateBox(glm::vec4(0.5f, 0.5f, 0.5f, PLANE_HALF_W));
    planeBuf_.init(planeMesh_);

    // --- Hoop faces: a hollow cube of half-size HOOP_OPENING in {X,Y,Z}, thin in W.
    // One mesh per thin-axis; each is instanced at + and - of that axis. ---
    faceXMesh_ = generateBox(glm::vec4(FACE_T, HOOP_OPENING, HOOP_OPENING, FACE_W));
    faceYMesh_ = generateBox(glm::vec4(HOOP_OPENING, FACE_T, HOOP_OPENING, FACE_W));
    faceZMesh_ = generateBox(glm::vec4(HOOP_OPENING, HOOP_OPENING, FACE_T, FACE_W));
    faceXBuf_.init(faceXMesh_);
    faceYBuf_.init(faceYMesh_);
    faceZBuf_.init(faceZMesh_);

    // --- Course: laid out by "flying" it. Starting from the default heading we apply a
    // compound turn (across all three W-planes), then advance `gap` along the new nose
    // and drop a gate there facing that nose. Because the turns left-multiply in world
    // space exactly like the controls, every gate's orientation is a reachable craft
    // heading — to thread a gate you must rotate your approach onto its facing. The
    // turns wander through X-W, Y-W and Z-W so the track uses every direction; the gaps
    // are long so each leg is a long trajectory. ~13 gates. ---
    Math4D::Rotor4D h = Math4D::Rotor4D::identity();
    glm::vec4 p(0.0f);
    auto addHoop = [&](float axDeg, float ayDeg, float azDeg, float gap) {
        const float ax = glm::radians(axDeg), ay = glm::radians(ayDeg), az = glm::radians(azDeg);
        h = Math4D::Rotor4D::fromXW(ax)
          * Math4D::Rotor4D::fromYW(ay)
          * Math4D::Rotor4D::fromZW(az)
          * h;
        h.normalize();
        glm::vec4 nose = h.reverse().toMatrix() * LOCAL_FWD;   // same nose convention as flight
        p += gap * nose;
        hoops_.push_back({p, h});
    };
    //        X-W    Y-W    Z-W    gap     // turn(s) exercised — gaps roomy so each
    //                                     // leg is a long, readable approach.
    addHoop(  0.0f,  0.0f,  0.0f, 56.0f);  // 1: straight — gentle start
    addHoop( 18.0f,  0.0f,  0.0f, 64.0f);  // 2: X-W bank (J/O)
    addHoop(  0.0f, 16.0f,  0.0f, 64.0f);  // 3: pitch up (I/K)
    addHoop(  0.0f,  0.0f, 20.0f, 64.0f);  // 4: Z-W (U/L)
    addHoop(-22.0f,  0.0f, 14.0f, 68.0f);  // 5: bank + Z
    addHoop(  0.0f,-20.0f,  0.0f, 64.0f);  // 6: pitch down
    addHoop( 20.0f, 18.0f,  0.0f, 68.0f);  // 7: bank + pitch
    addHoop(-16.0f,  0.0f,-22.0f, 68.0f);  // 8: bank + Z (other way)
    addHoop(  0.0f, 20.0f, 18.0f, 68.0f);  // 9: pitch + Z
    addHoop( 24.0f,-16.0f,  0.0f, 68.0f);  // 10: bank + pitch
    addHoop(-20.0f,  0.0f, 20.0f, 68.0f);  // 11: bank + Z
    addHoop(  0.0f,-18.0f,-20.0f, 68.0f);  // 12: pitch + Z
    addHoop( 16.0f, 16.0f, 16.0f, 68.0f);  // 13: full 4D compound — finish

    // --- Sky decorations: floating tesseracts scattered through the course volume. ---
    decorMesh_ = generateBox(glm::vec4(0.6f));
    decorBuf_.init(decorMesh_);
    glm::vec4 lo(hoops_.front().center), hi(hoops_.front().center);
    for (const auto& hp : hoops_) { lo = glm::min(lo, hp.center); hi = glm::max(hi, hp.center); }
    lo -= glm::vec4(10.0f); hi += glm::vec4(10.0f);
    unsigned seed = 9281u;
    auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return (float)((seed >> 8) & 0xFFFFFF) / (float)0x1000000;
    };
    const int DECOR_N = 60;
    for (int i = 0; i < DECOR_N; ++i) {
        glm::vec4 q(lo.x + (hi.x - lo.x) * rnd(),
                    lo.y + (hi.y - lo.y) * rnd(),
                    lo.z + (hi.z - lo.z) * rnd(),
                    lo.w + (hi.w - lo.w) * rnd());
        Math4D::Rotor4D ori = Math4D::Rotor4D::fromXW(rnd() * 6.2831853f)
                            * Math4D::Rotor4D::fromYZ(rnd() * 6.2831853f);
        decorInsts_.push_back({q, ori, DEC_A, DEC_B});
    }

    // --- Guide line: a polyline of GUIDE_SEGMENTS+1 points, refilled each frame. ---
    guideMesh_ = generatePolyline(GUIDE_SEGMENTS + 1);
    guideBuf_.init(guideMesh_);

    loaded_ = true;
}

// Build the trajectory guide: a cubic Bezier from the nose (tangent to the current
// heading) to the next gate's centre (tangent to the GATE's nose, the direction you
// must pass through it), sampled into GUIDE_SEGMENTS+1 points. Unlike Level 6 the curve
// is fully 4D — it follows the gate's real position and 4D facing. Empty if no gate left.
std::vector<glm::vec4> PlaneLevel4D::buildGuidePoints() const {
    if (currentHoop_ >= (int)hoops_.size()) return {};

    glm::vec4 nose = heading_.reverse().toMatrix() * LOCAL_FWD;
    float nl = glm::length(nose);
    if (nl < 1e-5f) return {};
    nose /= nl;

    const Hoop& g = hoops_[currentHoop_];
    glm::vec4 gnose = g.orient.reverse().toMatrix() * LOCAL_FWD;  // gate's pass-through dir
    float gl = glm::length(gnose);
    if (gl > 1e-5f) gnose /= gl;

    const glm::vec4 P0 = pos_ + GUIDE_NOSE_OUT * nose;     // emanate from the nose tip
    const glm::vec4 C  = g.center;                         // arrive at the centre
    float chord = glm::length(C - P0);
    if (chord < 1e-3f) return {};

    // Leave P0 along our nose; arrive at C along the gate's nose (handle sits "before"
    // C on the approach side). A generous handle gives a wide, readable sweep.
    float hh = GUIDE_HANDLE * chord;
    const glm::vec4 B0 = P0;
    const glm::vec4 B1 = P0 + nose  * hh;
    const glm::vec4 B2 = C  - gnose * hh;
    const glm::vec4 B3 = C;

    std::vector<glm::vec4> pts;
    pts.reserve(GUIDE_SEGMENTS + 1);
    for (int i = 0; i <= GUIDE_SEGMENTS; ++i) {
        float t = (float)i / (float)GUIDE_SEGMENTS;
        float u = 1.0f - t;
        glm::vec4 p = u*u*u * B0 + 3.0f*u*u*t * B1 + 3.0f*u*t*t * B2 + t*t*t * B3;
        pts.push_back(p);
    }
    return pts;
}

void PlaneLevel4D::update(const LevelContext& ctx) {
    if (ctx.insideMode && !won_) {
        // Three turns now — full 4D flight: X-W (J/O), Z-W (U/L) and, newly unlocked,
        // Y-W pitch (I/K). The three W-plane bivectors span all of SO(4), so the nose
        // can be aimed in any 4D direction to line up with each tilted gate.
        float a = glm::radians(TURN_SPEED * ctx.dt);
        if (glfwGetKey(ctx.window, GLFW_KEY_J) == GLFW_PRESS) heading_ = Math4D::Rotor4D::fromXW(-a) * heading_;
        if (glfwGetKey(ctx.window, GLFW_KEY_O) == GLFW_PRESS) heading_ = Math4D::Rotor4D::fromXW( a) * heading_;
        if (glfwGetKey(ctx.window, GLFW_KEY_U) == GLFW_PRESS) heading_ = Math4D::Rotor4D::fromZW(-a) * heading_;
        if (glfwGetKey(ctx.window, GLFW_KEY_L) == GLFW_PRESS) heading_ = Math4D::Rotor4D::fromZW( a) * heading_;
        if (glfwGetKey(ctx.window, GLFW_KEY_I) == GLFW_PRESS) heading_ = Math4D::Rotor4D::fromYW( a) * heading_;
        if (glfwGetKey(ctx.window, GLFW_KEY_K) == GLFW_PRESS) heading_ = Math4D::Rotor4D::fromYW(-a) * heading_;
        heading_.normalize();

        // Continuous forward thrust along the nose (local -W in world space). Y is free.
        glm::vec4 nose = heading_.reverse().toMatrix() * LOCAL_FWD;
        prevPos_ = pos_;
        pos_ += PLANE_SPEED * nose * ctx.dt;

        // Ordered gate pass: transform both endpoints of this frame's motion into the
        // gate's local frame (world = centre + M(orient.reverse())*local, so the inverse
        // is M(orient)). The gate's nose is local -W, so a forward crossing is local w
        // going + -> -; "inside" is the local X/Y/Z opening. A miss isn't punished.
        if (currentHoop_ < (int)hoops_.size()) {
            const Hoop& g = hoops_[currentHoop_];
            glm::mat4 toLocal = g.orient.toMatrix();
            glm::vec4 lc = toLocal * (pos_     - g.center);
            glm::vec4 lp = toLocal * (prevPos_ - g.center);
            bool crossed = lp.w > 0.0f && lc.w <= 0.0f;
            bool inside  = std::abs(lc.x) < HOOP_OPENING
                        && std::abs(lc.y) < HOOP_OPENING
                        && std::abs(lc.z) < HOOP_OPENING;
            if (crossed && inside) {
                ++currentHoop_;
                if (currentHoop_ >= (int)hoops_.size())
                    won_ = true;
            }
        }
    } else if (!ctx.insideMode) {
        cam3D_.processInput(ctx.window, ctx.dt);
    }

    // --- Chase camera: trail behind + above along the FULL nose (pitch is live now, so
    // "behind" can no longer be the horizontal projection). We drive the follow-cam's
    // orientation straight off the craft heading (yaw carries the whole 4D rotor here;
    // the flat-horizon restriction only matters inside Camera4D::processInput, which we
    // bypass) plus a small fixed downward tilt. ---
    glm::vec4 nose = heading_.reverse().toMatrix() * LOCAL_FWD;
    const glm::vec4 worldUp(0.0f, 1.0f, 0.0f, 0.0f);
    glm::vec4 targetPos = pos_ - FOLLOW_DIST * nose + FOLLOW_HEIGHT * worldUp;
    Math4D::Rotor4D targetYaw = heading_;
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
}

void PlaneLevel4D::render(const LevelContext& ctx) {
    const Math4D::Rotor4D ori = cam4D_.getOrientation();

    // Relax the depth fog for this level: with gates strung far down the track, the
    // global fog would swallow them long before you could read them.
    RenderSettings vis = largeScene(ctx.vis, 120.0f, 0.18f);

    auto draw = [&](const std::vector<ObjectInstance>& insts, const Object4D& mesh, ObjectBuffer& buf) {
        ctx.renderer.drawObjects(insts, mesh, buf, cam4D_, ori, focal_, ctx.innerMVP, vis);
    };

    // Sky decorations first (farthest context).
    draw(decorInsts_, decorMesh_, decorBuf_);

    // Gates: each is a hollow cube tilted by its orientation. We rotate the six face
    // panels together — offset each in the gate's local frame then place it with the
    // gate's rotor (matching how the panel mesh is itself rotated). Colour by state.
    const float R = HOOP_OPENING;
    std::vector<ObjectInstance> fx, fy, fz;
    for (size_t i = 0; i < hoops_.size(); ++i) {
        glm::vec3 col = ((int)i <  currentHoop_) ? HOOP_PASSED
                      : ((int)i == currentHoop_) ? HOOP_ACTIVE
                                                 : HOOP_NEXT;
        const Hoop& g = hoops_[i];
        const Math4D::Rotor4D q = g.orient.reverse();
        const glm::mat4 qM = q.toMatrix();
        auto panel = [&](float dx, float dy, float dz) -> ObjectInstance {
            glm::vec4 world = g.center + qM * glm::vec4(dx, dy, dz, 0.0f);
            return {world, q, col, col};
        };
        fx.push_back(panel( R, 0.0f, 0.0f)); fx.push_back(panel(-R, 0.0f, 0.0f));
        fy.push_back(panel(0.0f,  R, 0.0f)); fy.push_back(panel(0.0f, -R, 0.0f));
        fz.push_back(panel(0.0f, 0.0f,  R)); fz.push_back(panel(0.0f, 0.0f, -R));
    }
    draw(fx, faceXMesh_, faceXBuf_);
    draw(fy, faceYMesh_, faceYBuf_);
    draw(fz, faceZMesh_, faceZBuf_);

    // Trajectory guide: curve from the nose to the active gate's centre. Recomputed live.
    std::vector<glm::vec4> guide = buildGuidePoints();
    if (!guide.empty()) {
        guideMesh_.vertices = guide;  // count matches the buffer (GUIDE_SEGMENTS+1)
        ctx.renderer.drawPolyline(guideMesh_, guideBuf_, cam4D_, ori, focal_,
                                  ctx.innerMVP, vis, GUIDE_COLOR, 2.5f);
    }

    // Plane last: warm/cool W gradient. Rendered with heading_.reverse() so the mesh's
    // orientation matches the convention the velocity, nose and camera all use.
    std::vector<ObjectInstance> planeInst = {{pos_, heading_.reverse(), AV_WARM, AV_COOL}};
    draw(planeInst, planeMesh_, planeBuf_);

    ctx.renderer.drawOuterCube(ctx.outerMVP);
}

void PlaneLevel4D::renderHUD(const LevelContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(440, 200), ImGuiCond_Always);
    ImGui::Begin("Plane 4D", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("Plane (4D course) - constant forward thrust; thread the gates in order.");
    if (!ctx.insideMode) {
        ImGui::TextWrapped("Press TAB to step inside the 4D view.");
    } else {
        ImGui::TextWrapped("J/O turn (X-W), U/L turn (Z-W), I/K pitch (Y-W) - all three turns, "
                           "genuine 4D flight. The gates are tilted: rotate to line your approach "
                           "up with each gate's facing, not just its position.");
        ImGui::TextWrapped("Miss one? Bank around and come back through it.");
        ImGui::TextWrapped("Gate %d / %zu.", std::min(currentHoop_ + 1, (int)hoops_.size()),
                           hoops_.size());
    }
    if (won_)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "All gates cleared - level complete!");
    ImGui::End();

    glm::vec4 fw = glm::inverse(cam4D_.getOrientation().toMatrix()) * glm::vec4(0, 0, 0, -1);
    hud::drawFacingWidget(fw);
    hud::drawCoordWidget(pos_, "Plane");
}
