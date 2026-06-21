#include "levels/PlaneLevel.h"

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
}

PlaneLevel::PlaneLevel() {
    // Start at the origin, level height, facing -W (the shared default heading).
    pos_     = glm::vec4(0.0f, FLIGHT_Y, 0.0f, 0.0f);
    heading_ = Math4D::Rotor4D::identity();
}

PlaneLevel::~PlaneLevel() {
    planeBuf_.destroy();
    faceXBuf_.destroy();
    faceYBuf_.destroy();
    faceZBuf_.destroy();
    decorBuf_.destroy();
    guideBuf_.destroy();
}

void PlaneLevel::load() {
    // --- Plane: a box stretched long along W (its forward/back axis) so it reads as
    // a slender dart pointing down its flight line, not a chunky cube. ---
    planeMesh_ = generateBox(glm::vec4(0.5f, 0.5f, 0.5f, PLANE_HALF_W));
    planeBuf_.init(planeMesh_);

    // --- Hoop faces: a hoop is a hollow cube of half-size HOOP_OPENING in {X,Y,Z},
    // thin in W. Each face panel spans the cube in two axes and is thin in the third
    // (and thin in W). One mesh per thin-axis; each is instanced at + and -. ---
    faceXMesh_ = generateBox(glm::vec4(FACE_T, HOOP_OPENING, HOOP_OPENING, FACE_W));
    faceYMesh_ = generateBox(glm::vec4(HOOP_OPENING, FACE_T, HOOP_OPENING, FACE_W));
    faceZMesh_ = generateBox(glm::vec4(HOOP_OPENING, HOOP_OPENING, FACE_T, FACE_W));
    faceXBuf_.init(faceXMesh_);
    faceYBuf_.init(faceYMesh_);
    faceZBuf_.init(faceZMesh_);

    // --- Course: hoops strung along -W, each offset in X and/or Z so the pilot must
    // use the horizontal turns (J/O for X, U/L for Z) to line up — a 3D slalom. The
    // first few isolate one axis; later ones combine them. ---
    float cw = 0.0f, cx = 0.0f, cz = 0.0f;
    auto addHoop = [&](float dx, float dz, float gap) {
        cw -= gap;          // next hoop further forward (-W)
        cx += dx;
        cz += dz;
        hoops_.push_back({glm::vec4(cx, FLIGHT_Y, cz, cw)});
    };
    addHoop( 0.0f,  0.0f, 30.0f);  // straight ahead — gentle start
    addHoop( 2.6f,  0.0f, 34.0f);  // right only (J/O)
    addHoop( 0.0f,  3.0f, 34.0f);  // ana only (U/L)
    addHoop(-2.8f,  0.0f, 34.0f);  // left only
    addHoop( 0.0f, -3.2f, 34.0f);  // kata only
    addHoop( 3.0f,  2.2f, 34.0f);  // right + ana
    addHoop(-2.6f, -2.6f, 34.0f);  // left + kata
    addHoop( 0.0f,  3.0f, 34.0f);  // ana — finish

    // --- Sky decorations: floating tesseracts scattered through the course volume. ---
    decorMesh_ = generateBox(glm::vec4(0.6f));
    decorBuf_.init(decorMesh_);
    const float wMax = 4.0f;
    const float wMin = hoops_.back().center.w - 6.0f;
    unsigned seed = 9281u;
    auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return (float)((seed >> 8) & 0xFFFFFF) / (float)0x1000000;
    };
    const int DECOR_N = 40;
    for (int i = 0; i < DECOR_N; ++i) {
        float w = wMax + (wMin - wMax) * rnd();
        float x = (rnd() - 0.5f) * 48.0f;
        float z = (rnd() - 0.5f) * 48.0f;
        float y = FLIGHT_Y + (rnd() - 0.5f) * 16.0f;
        Math4D::Rotor4D ori = Math4D::Rotor4D::fromXW(rnd() * 6.2831853f)
                            * Math4D::Rotor4D::fromYZ(rnd() * 6.2831853f);
        decorInsts_.push_back({glm::vec4(x, y, z, w), ori, DEC_A, DEC_B});
    }

    // --- Guide line: a polyline of GUIDE_SEGMENTS+1 points, refilled each frame. ---
    guideMesh_ = generatePolyline(GUIDE_SEGMENTS + 1);
    guideBuf_.init(guideMesh_);

    loaded_ = true;
}

// Build the trajectory guide: a cubic Bezier from the nose (tangent to the current
// heading) to the next hoop's centre (tangent to -W, the pass-through direction),
// sampled into GUIDE_SEGMENTS+1 points. The whole curve lives at Y = 0 since the
// nose, the hoop centres and -W all have no Y component. Empty when no hoop remains.
std::vector<glm::vec4> PlaneLevel::buildGuidePoints() const {
    if (currentHoop_ >= (int)hoops_.size()) return {};

    glm::vec4 nose = heading_.reverse().toMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
    float nl = glm::length(nose);
    if (nl < 1e-5f) return {};
    nose /= nl;

    const glm::vec4 P0 = pos_ + GUIDE_NOSE_OUT * nose;     // emanate from the nose tip
    const glm::vec4 C  = hoops_[currentHoop_].center;      // arrive at the centre
    float chord = glm::length(C - P0);
    if (chord < 1e-3f) return {};

    // Control points: leave P0 along the nose, arrive at C along -W (so the handle
    // before C sits on the +W side). A generous handle gives a wide, readable sweep.
    float h = GUIDE_HANDLE * chord;
    const glm::vec4 B0 = P0;
    const glm::vec4 B1 = P0 + nose * h;
    const glm::vec4 B2 = C + glm::vec4(0.0f, 0.0f, 0.0f, h);  // = C - (-W) * h
    const glm::vec4 B3 = C;

    std::vector<glm::vec4> pts;
    pts.reserve(GUIDE_SEGMENTS + 1);
    for (int i = 0; i <= GUIDE_SEGMENTS; ++i) {
        float t = (float)i / (float)GUIDE_SEGMENTS;
        float u = 1.0f - t;
        glm::vec4 p = u*u*u * B0 + 3.0f*u*u*t * B1 + 3.0f*u*t*t * B2 + t*t*t * B3;
        p.y = FLIGHT_Y;  // keep the guide exactly in the flight plane
        pts.push_back(p);
    }
    return pts;
}

void PlaneLevel::update(const LevelContext& ctx) {
    if (ctx.insideMode && !won_) {
        // Two horizontal turns only: X-W (J/O) and Z-W (U/L). These bivector planes
        // never involve Y, so the nose stays level and the plane holds its height —
        // pitch (Y-W) is deliberately not bound. (Mirrors Camera4D's yaw composition.)
        float a = glm::radians(TURN_SPEED * ctx.dt);
        if (glfwGetKey(ctx.window, GLFW_KEY_J) == GLFW_PRESS) heading_ = Math4D::Rotor4D::fromXW(-a) * heading_;
        if (glfwGetKey(ctx.window, GLFW_KEY_O) == GLFW_PRESS) heading_ = Math4D::Rotor4D::fromXW( a) * heading_;
        if (glfwGetKey(ctx.window, GLFW_KEY_U) == GLFW_PRESS) heading_ = Math4D::Rotor4D::fromZW(-a) * heading_;
        if (glfwGetKey(ctx.window, GLFW_KEY_L) == GLFW_PRESS) heading_ = Math4D::Rotor4D::fromZW( a) * heading_;
        heading_.normalize();

        // Continuous forward thrust along the nose (local -W in world space).
        glm::vec4 nose = heading_.reverse().toMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
        float prevW = pos_.w;
        pos_ += PLANE_SPEED * nose * ctx.dt;
        pos_.y = FLIGHT_Y;   // guard against any numerical drift into Y

        // Ordered hoop pass: when we cross the current hoop's W plane heading -W AND
        // are inside its opening, count it. A miss isn't punished — keep flying and
        // bank back around to re-approach (the crossing only fires going -W).
        if (currentHoop_ < (int)hoops_.size()) {
            const glm::vec4 c = hoops_[currentHoop_].center;
            bool crossed = prevW > c.w && pos_.w <= c.w;
            bool inside  = std::abs(pos_.x - c.x) < HOOP_OPENING
                        && std::abs(pos_.z - c.z) < HOOP_OPENING;
            if (crossed && inside) {
                ++currentHoop_;
                if (currentHoop_ >= (int)hoops_.size())
                    won_ = true;
            }
        }
    } else if (!ctx.insideMode) {
        cam3D_.processInput(ctx.window, ctx.dt);
    }

    // --- Chase camera: trail behind + above, looking gently down (reused from Level 4).
    // The nose has no Y component, so "behind" is simply -nose; the eye sits FOLLOW_DIST
    // behind and FOLLOW_HEIGHT above, aimed CAM_AIM_HEIGHT above the plane.
    glm::vec4 nose = heading_.reverse().toMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
    glm::vec4 fh(nose.x, 0.0f, nose.z, nose.w);
    float fl = glm::length(fh);
    if (fl > 1e-4f) fh /= fl;
    glm::vec4 targetPos = pos_ - FOLLOW_DIST * fh + glm::vec4(0.0f, FOLLOW_HEIGHT, 0.0f, 0.0f);
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

void PlaneLevel::render(const LevelContext& ctx) {
    const Math4D::Rotor4D ori = cam4D_.getOrientation();
    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();

    // Relax the depth fog for this level only: with hoops strung far down -W, the
    // global fog would swallow them long before you could read them. Push the fog
    // window far out and soften its strength so distant hoops stay clearly visible.
    RenderSettings vis = largeScene(ctx.vis, 120.0f, 0.18f);

    auto draw = [&](const std::vector<ObjectInstance>& insts, const Object4D& mesh, ObjectBuffer& buf) {
        ctx.renderer.drawObjects(insts, mesh, buf, cam4D_, ori, focal_, ctx.innerMVP, vis);
    };

    // Sky decorations first (farthest context).
    draw(decorInsts_, decorMesh_, decorBuf_);

    // Hoops: build the six face panels of every hollow cube, grouped per face-mesh so
    // the renderer depth-sorts them together. Colour each by its hoop's state.
    const float R = HOOP_OPENING;
    std::vector<ObjectInstance> fx, fy, fz;
    for (size_t i = 0; i < hoops_.size(); ++i) {
        glm::vec3 col = ((int)i <  currentHoop_) ? HOOP_PASSED
                      : ((int)i == currentHoop_) ? HOOP_ACTIVE
                                                 : HOOP_NEXT;
        const glm::vec4 c = hoops_[i].center;
        fx.push_back({c + glm::vec4( R, 0.0f, 0.0f, 0.0f), I, col, col});
        fx.push_back({c + glm::vec4(-R, 0.0f, 0.0f, 0.0f), I, col, col});
        fy.push_back({c + glm::vec4(0.0f,  R, 0.0f, 0.0f), I, col, col});
        fy.push_back({c + glm::vec4(0.0f, -R, 0.0f, 0.0f), I, col, col});
        fz.push_back({c + glm::vec4(0.0f, 0.0f,  R, 0.0f), I, col, col});
        fz.push_back({c + glm::vec4(0.0f, 0.0f, -R, 0.0f), I, col, col});
    }
    draw(fx, faceXMesh_, faceXBuf_);
    draw(fy, faceYMesh_, faceYBuf_);
    draw(fz, faceZMesh_, faceZBuf_);

    // Trajectory guide: curve from the nose to the active hoop's centre (always on
    // top of geom mode). Recomputed live from the current position/heading.
    std::vector<glm::vec4> guide = buildGuidePoints();
    if (!guide.empty()) {
        guideMesh_.vertices = guide;  // count matches the buffer (GUIDE_SEGMENTS+1)
        ctx.renderer.drawPolyline(guideMesh_, guideBuf_, cam4D_, ori, focal_,
                                  ctx.innerMVP, vis, GUIDE_COLOR, 2.5f);
    }

    // Plane last: warm/cool W gradient. Render with heading_.reverse() so the mesh's
    // orientation matches the convention the velocity, nose and camera all use
    // (forward = M(R.reverse())*(-W)); otherwise camOri * inst.orientation fails to
    // cancel and the plane drifts in the view as you turn (camera-follow breaks).
    std::vector<ObjectInstance> planeInst = {{pos_, heading_.reverse(), AV_WARM, AV_COOL}};
    draw(planeInst, planeMesh_, planeBuf_);

    ctx.renderer.drawOuterCube(ctx.outerMVP);
}

void PlaneLevel::renderHUD(const LevelContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(440, 180), ImGuiCond_Always);
    ImGui::Begin("Plane", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("Plane - constant forward thrust; fly through the hoops in order.");
    if (!ctx.insideMode) {
        ImGui::TextWrapped("Press TAB to step inside the 4D view.");
    } else {
        ImGui::TextWrapped("J/O and U/L steer (two horizontal turns). You can't climb or dive - "
                           "with height fixed, it flies just like a 3D plane.");
        ImGui::TextWrapped("Miss one? Bank around and come back through it.");
        ImGui::TextWrapped("Hoop %d / %zu.", std::min(currentHoop_ + 1, (int)hoops_.size()),
                           hoops_.size());
    }
    if (won_)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "All hoops cleared - level complete!");
    ImGui::End();

    glm::vec4 fw = glm::inverse(cam4D_.getOrientation().toMatrix()) * glm::vec4(0, 0, 0, -1);
    hud::drawFacingWidget(fw);
    hud::drawCoordWidget(pos_, "Plane");
}
