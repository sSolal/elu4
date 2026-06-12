// Headless tests for the 4D rotation core (src/Math4D.h).
//
// These tests need NO OpenGL/GLFW — Math4D.h depends only on GLM. The strategy:
// the simple inline rotXX() 2x2 rotations are taken as an *independent oracle*
// (they are trivially correct standard 2D rotations), and the fancy Rotor4D
// geometric-algebra path is pinned against them plus a set of mathematical
// invariants (norm preservation, orthonormality, inverse round-trips, the
// "rotate a lot in many directions" stress test that used to break the game).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../src/third_party/doctest.h"

#include "../src/Math4D.h"
#include <glm/glm.hpp>
#include <array>
#include <string>
#include <cmath>

using Math4D::Rotor4D;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

constexpr float EPS = 1e-4f;

// The 6 rotation planes, each with its rotor factory and the matching inline
// 2x2 oracle. The oracle mutates the relevant pair of components of a vec4.
struct Plane {
    const char* name;
    Rotor4D (*factory)(float);
    void (*oracle)(glm::vec4&, float c, float s);
};

void oracleXY(glm::vec4& v, float c, float s) { Math4D::rotXY(v.x, v.y, c, s); }
void oracleXZ(glm::vec4& v, float c, float s) { Math4D::rotXZ(v.x, v.z, c, s); }
void oracleXW(glm::vec4& v, float c, float s) { Math4D::rotXW(v.x, v.w, c, s); }
void oracleYZ(glm::vec4& v, float c, float s) { Math4D::rotYZ(v.y, v.z, c, s); }
void oracleYW(glm::vec4& v, float c, float s) { Math4D::rotYW(v.y, v.w, c, s); }
void oracleZW(glm::vec4& v, float c, float s) { Math4D::rotZW(v.z, v.w, c, s); }

const std::array<Plane, 6> PLANES = {{
    {"XY", &Rotor4D::fromXY, &oracleXY},
    {"XZ", &Rotor4D::fromXZ, &oracleXZ},
    {"XW", &Rotor4D::fromXW, &oracleXW},
    {"YZ", &Rotor4D::fromYZ, &oracleYZ},
    {"YW", &Rotor4D::fromYW, &oracleYW},
    {"ZW", &Rotor4D::fromZW, &oracleZW},
}};

// Apply the oracle for a plane at a given angle (builds c/s like the CLI does).
glm::vec4 applyOracle(const Plane& pl, glm::vec4 v, float angle) {
    pl.oracle(v, std::cos(angle), std::sin(angle));
    return v;
}

float rotorNorm(const Rotor4D& r) {
    return std::sqrt(r.s*r.s + r.b12*r.b12 + r.b13*r.b13 + r.b14*r.b14 +
                     r.b23*r.b23 + r.b24*r.b24 + r.b34*r.b34 + r.p*r.p);
}

void checkVecEq(const glm::vec4& a, const glm::vec4& b, float eps = EPS) {
    CHECK(a.x == doctest::Approx(b.x).epsilon(eps));
    CHECK(a.y == doctest::Approx(b.y).epsilon(eps));
    CHECK(a.z == doctest::Approx(b.z).epsilon(eps));
    CHECK(a.w == doctest::Approx(b.w).epsilon(eps));
}

// A small spread of test points and angles reused across cases.
const std::array<glm::vec4, 4> POINTS = {{
    {1, 0, 0, 0}, {0.3f, -0.7f, 0.5f, 0.2f}, {1, 1, 1, 1}, {-0.4f, 0.9f, -0.2f, 0.6f},
}};
const std::array<float, 5> ANGLES = {{0.1f, 0.7f, 1.5708f, 2.5f, -1.2f}};

} // namespace

// ---------------------------------------------------------------------------
// Identity, inverse, associativity
// ---------------------------------------------------------------------------

TEST_CASE("identity rotor is the multiplicative identity") {
    Rotor4D I = Rotor4D::identity();
    for (const auto& pl : PLANES) {
        Rotor4D R = pl.factory(0.9f);
        Rotor4D IR = I * R;
        Rotor4D RI = R * I;
        CHECK(IR.s == doctest::Approx(R.s));
        CHECK(RI.s == doctest::Approx(R.s));
        CHECK((I * R).b12 == doctest::Approx(R.b12));
    }
}

TEST_CASE("R * reverse(R) == identity") {
    for (const auto& pl : PLANES) {
        for (float a : ANGLES) {
            Rotor4D R = pl.factory(a);
            Rotor4D prod = R * R.reverse();
            CHECK(prod.s == doctest::Approx(1.0f).epsilon(EPS));
            CHECK(std::abs(prod.b12) < EPS);
            CHECK(std::abs(prod.b13) < EPS);
            CHECK(std::abs(prod.b14) < EPS);
            CHECK(std::abs(prod.b23) < EPS);
            CHECK(std::abs(prod.b24) < EPS);
            CHECK(std::abs(prod.b34) < EPS);
            CHECK(std::abs(prod.p) < EPS);
        }
    }
}

TEST_CASE("geometric product is associative") {
    Rotor4D A = Rotor4D::fromXY(0.6f);
    Rotor4D B = Rotor4D::fromZW(1.1f);
    Rotor4D C = Rotor4D::fromXW(-0.4f);
    Rotor4D left  = (A * B) * C;
    Rotor4D right = A * (B * C);
    CHECK(left.s   == doctest::Approx(right.s));
    CHECK(left.b12 == doctest::Approx(right.b12));
    CHECK(left.b34 == doctest::Approx(right.b34));
    CHECK(left.p   == doctest::Approx(right.p));
}

// ---------------------------------------------------------------------------
// Oracle agreement: each single-plane rotor matches the inline 2x2 rotation
// ---------------------------------------------------------------------------

TEST_CASE("single-plane rotor matches inline 2x2 oracle") {
    for (const auto& pl : PLANES) {
        for (float a : ANGLES) {
            glm::mat4 M = pl.factory(a).toMatrix();
            for (const auto& p : POINTS) {
                glm::vec4 viaRotor = M * p;
                glm::vec4 viaOracle = applyOracle(pl, p, a);
                INFO("plane=", pl.name, " angle=", a);
                checkVecEq(viaRotor, viaOracle);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Cross-plane composition: THE regression for "rotate in many directions".
// Composing rotors must equal applying the inline rotations in sequence.
// R = fromA(a) * fromB(b)  means "apply B first, then A".
// ---------------------------------------------------------------------------

TEST_CASE("cross-plane composition matches sequential oracle rotations") {
    const float a = 0.8f, b = -1.3f;
    for (const auto& A : PLANES) {
        for (const auto& B : PLANES) {
            Rotor4D R = A.factory(a) * B.factory(b);
            glm::mat4 M = R.toMatrix();
            for (const auto& p : POINTS) {
                glm::vec4 viaRotor = M * p;
                glm::vec4 viaOracle = applyOracle(A, applyOracle(B, p, b), a);
                INFO("A=", A.name, " B=", B.name);
                checkVecEq(viaRotor, viaOracle, 2e-4f);
            }
        }
    }
}

TEST_CASE("toMatrix is a homomorphism: M(A*B) == M(A)*M(B)") {
    Rotor4D A = Rotor4D::fromXW(0.5f);
    Rotor4D B = Rotor4D::fromYZ(1.0f);
    glm::mat4 composed = (A * B).toMatrix();
    glm::mat4 multiplied = A.toMatrix() * B.toMatrix();
    for (int c = 0; c < 4; ++c)
        checkVecEq(composed[c], multiplied[c]);
}

// ---------------------------------------------------------------------------
// Invariants: unit norm, isometry (length & distance preserved)
// ---------------------------------------------------------------------------

TEST_CASE("product of unit rotors stays unit") {
    Rotor4D R = Rotor4D::identity();
    for (const auto& pl : PLANES)
        R = pl.factory(0.9f) * R;
    CHECK(rotorNorm(R) == doctest::Approx(1.0f).epsilon(EPS));
}

TEST_CASE("rotation preserves vector length and pairwise distance") {
    Rotor4D R = Rotor4D::fromXY(0.7f) * Rotor4D::fromZW(1.2f) * Rotor4D::fromXW(-0.5f);
    glm::mat4 M = R.toMatrix();
    for (const auto& p : POINTS) {
        glm::vec4 rp = M * p;
        CHECK(glm::length(rp) == doctest::Approx(glm::length(p)).epsilon(EPS));
    }
    glm::vec4 d  = POINTS[1] - POINTS[2];
    glm::vec4 rd = (M * POINTS[1]) - (M * POINTS[2]);
    CHECK(glm::length(rd) == doctest::Approx(glm::length(d)).epsilon(EPS));
}

// ---------------------------------------------------------------------------
// Stress test: "rotate a lot in many directions" — the original failure mode.
// A long DETERMINISTIC sequence of mixed-plane rotations must keep the rotor
// unit and the matrix orthonormal (M^T M == I).
// ---------------------------------------------------------------------------

TEST_CASE("stress: thousands of mixed-plane rotations stay orthonormal") {
    Rotor4D R = Rotor4D::identity();
    // Deterministic pseudo-sequence (no rand/time): vary plane & angle by index.
    for (int i = 0; i < 5000; ++i) {
        const Plane& pl = PLANES[i % 6];
        float angle = 0.05f + 0.013f * static_cast<float>((i * 7919) % 97);
        R = pl.factory(angle) * R;
        R.normalize();  // mirrors the fix applied in the Visualizer
    }

    CHECK(rotorNorm(R) == doctest::Approx(1.0f).epsilon(EPS));

    glm::mat4 M = R.toMatrix();
    glm::mat4 MtM = glm::transpose(M) * M;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float expected = (r == c) ? 1.0f : 0.0f;
            INFO("MtM[", r, "][", c, "]");
            CHECK(MtM[c][r] == doctest::Approx(expected).epsilon(1e-3f));
        }
    }
}

TEST_CASE("stress without normalization still composes correctly (math, not drift)") {
    // Even without normalize(), a MODEST number of exact compositions should
    // remain near-unit if the geometric product is correct. This isolates
    // "algebra is wrong" (fast blow-up) from "float drift" (slow).
    Rotor4D R = Rotor4D::identity();
    for (int i = 0; i < 200; ++i)
        R = PLANES[i % 6].factory(0.3f) * R;
    CHECK(rotorNorm(R) == doctest::Approx(1.0f).epsilon(1e-2f));
}
