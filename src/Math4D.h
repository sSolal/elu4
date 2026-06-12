#pragma once

#include <glm/glm.hpp>
#include <cmath>

namespace Math4D {

// 4D Rotor: even subalgebra of Clifford algebra Cl(4,0)
// Represents rotations via geometric product in 4D space
// Components: scalar (grade 0), bivectors (grade 2), pseudoscalar (grade 4)
struct Rotor4D {
    float s;                            // scalar (grade 0)
    float b12, b13, b14, b23, b24, b34; // bivectors: XY XZ XW YZ YW ZW
    float p;                            // pseudoscalar e1234 (grade 4)

    // Identity rotor (no rotation)
    static Rotor4D identity() {
        return {1.0f, 0,0,0,0,0,0, 0};
    }

    // Factory functions: create rotor for rotation in each plane
    // Each verifies sign convention against existing Math4D::rot* functions

    static Rotor4D fromXY(float angle) {
        float h = angle * 0.5f;
        float c = std::cos(h);
        float s = -std::sin(h);  // negative to match standard CCW convention
        return {c, s,0,0,0,0,0, 0};
    }

    static Rotor4D fromXZ(float angle) {
        float h = angle * 0.5f;
        float c = std::cos(h);
        float s = -std::sin(h);  // negative to match standard CCW convention
        return {c, 0,s,0,0,0,0, 0};
    }

    static Rotor4D fromXW(float angle) {
        float h = angle * 0.5f;
        float c = std::cos(h);
        float s = std::sin(h);   // positive: XW has special W signature (c*x + s*w, -s*x + c*w)
        return {c, 0,0,s,0,0,0, 0};
    }

    static Rotor4D fromYZ(float angle) {
        float h = angle * 0.5f;
        float c = std::cos(h);
        float s = -std::sin(h);  // negative to match standard CCW convention
        return {c, 0,0,0,s,0,0, 0};
    }

    static Rotor4D fromYW(float angle) {
        float h = angle * 0.5f;
        float c = std::cos(h);
        float s = std::sin(h);   // positive: YW has special W signature
        return {c, 0,0,0,0,s,0, 0};
    }

    static Rotor4D fromZW(float angle) {
        float h = angle * 0.5f;
        float c = std::cos(h);
        float s = std::sin(h);   // positive: ZW has special W signature
        return {c, 0,0,0,0,0,s, 0};
    }

    // Geometric product: compose two rotors.
    // Full multiplication table for the even subalgebra of Cl(4,0), derived from
    // the basis relations e_i e_i = +1, e_i e_j = -e_j e_i. Bivectors sharing one
    // index anticommute (e13*e12 = +e23, e12*e13 = -e23); disjoint bivectors
    // commute into the pseudoscalar (e12*e34 = I, e13*e24 = -I, e14*e23 = I);
    // the pseudoscalar duals bivectors (I*e12 = -e34, I*e13 = +e24, I*e14 = -e23).
    Rotor4D operator*(const Rotor4D& r) const {
        Rotor4D out{};
        // Scalar (grade 0): e_ij^2 = -1, I^2 = +1
        out.s = s*r.s - b12*r.b12 - b13*r.b13 - b14*r.b14 - b23*r.b23 - b24*r.b24 - b34*r.b34 + p*r.p;

        // b12 (e12)
        out.b12 = s*r.b12 + b12*r.s - b13*r.b23 + b23*r.b13 - b14*r.b24 + b24*r.b14 - b34*r.p - p*r.b34;

        // b13 (e13)
        out.b13 = s*r.b13 + b13*r.s + b12*r.b23 - b23*r.b12 - b14*r.b34 + b34*r.b14 + b24*r.p + p*r.b24;

        // b14 (e14)
        out.b14 = s*r.b14 + b14*r.s + b12*r.b24 - b24*r.b12 + b13*r.b34 - b34*r.b13 - b23*r.p - p*r.b23;

        // b23 (e23)
        out.b23 = s*r.b23 + b23*r.s - b12*r.b13 + b13*r.b12 - b24*r.b34 + b34*r.b24 - b14*r.p - p*r.b14;

        // b24 (e24)
        out.b24 = s*r.b24 + b24*r.s - b12*r.b14 + b14*r.b12 + b23*r.b34 - b34*r.b23 + b13*r.p + p*r.b13;

        // b34 (e34)
        out.b34 = s*r.b34 + b34*r.s - b13*r.b14 + b14*r.b13 - b23*r.b24 + b24*r.b23 - b12*r.p - p*r.b12;

        // p (e1234 = pseudoscalar)
        out.p = s*r.p + p*r.s + b12*r.b34 + b34*r.b12 - b13*r.b24 - b24*r.b13 + b14*r.b23 + b23*r.b14;

        return out;
    }

    // Reverse: flip sign of all bivectors and pseudoscalar
    // pseudoscalar is self-reverse: (-1)^(4*3/2) = (-1)^6 = +1
    Rotor4D reverse() const {
        return {s, -b12,-b13,-b14,-b23,-b24,-b34, p};
    }

    // Normalize to unit rotor (Euclidean norm)
    void normalize() {
        float n = std::sqrt(s*s + b12*b12 + b13*b13 + b14*b14 + b23*b23 + b24*b24 + b34*b34 + p*p);
        if (n > 1e-6f) {
            s /= n; b12 /= n; b13 /= n; b14 /= n; b23 /= n; b24 /= n; b34 /= n; p /= n;
        }
    }

public:
    // Extract 4x4 rotation matrix via full sandwich product R*v*R†
    // Derived by expanding R*v (even × grade-1 → grade-1 + grade-3) then
    // multiplying by R† to extract grade-1 output.
    glm::mat4 toMatrix() const {
        // Apply R*v*R† to a single vector
        auto applyTo = [&](float v1, float v2, float v3, float v4) -> glm::vec4 {
            // Step 1: T = R * v  (grade-1 + grade-3 result)
            float t1   =  s*v1 + b12*v2 + b13*v3 + b14*v4;
            float t2   =  s*v2 - b12*v1 + b23*v3 + b24*v4;
            float t3   =  s*v3 - b13*v1 - b23*v2 + b34*v4;
            float t4   =  s*v4 - b14*v1 - b24*v2 - b34*v3;
            float t123 =  b12*v3 - b13*v2 + b23*v1 + p*v4;
            float t124 =  b12*v4 - b14*v2 + b24*v1 - p*v3;
            float t134 =  b13*v4 - b14*v3 + b34*v1 + p*v2;
            float t234 =  b23*v4 - b24*v3 + b34*v2 - p*v1;

            // Step 2: grade-1 part of T * R†  (R† = s - bivectors + p*e1234)
            float re1 = s*t1  + b12*t2  + b13*t3  + b14*t4  + b23*t123 + b24*t124 + b34*t134 + p*t234;
            float re2 = s*t2  - b12*t1  + b23*t3  + b24*t4  - b13*t123 - b14*t124 + b34*t234 - p*t134;
            float re3 = s*t3  - b13*t1  - b23*t2  + b34*t4  + b12*t123 - b14*t134 - b24*t234 + p*t124;
            float re4 = s*t4  - b14*t1  - b24*t2  - b34*t3  + b12*t124 + b13*t134 + b23*t234 - p*t123;
            return {re1, re2, re3, re4};
        };

        // Columns = where each basis vector maps to
        return glm::mat4(
            applyTo(1,0,0,0),
            applyTo(0,1,0,0),
            applyTo(0,0,1,0),
            applyTo(0,0,0,1)
        );
    }

    // Sandwich product: R*v*R† rotates vector v
    glm::vec4 rotate(glm::vec4 v) const {
        return toMatrix() * v;
    }
};

// Inline 4D rotation functions: mutate a vec4 in-place using pre-computed cos/sin
// These apply rotations in the specified planes using the angle representation.
// (Kept for backward compatibility; new code should use Rotor4D)

inline void rotXY(float& vx, float& vy, float c, float s) {
    float nx = c * vx - s * vy;
    float ny = s * vx + c * vy;
    vx = nx;
    vy = ny;
}

inline void rotXZ(float& vx, float& vz, float c, float s) {
    float nx = c * vx - s * vz;
    float nz = s * vx + c * vz;
    vx = nx;
    vz = nz;
}

inline void rotXW(float& vx, float& vw, float c, float s) {
    float nx = c * vx + s * vw;
    float nw = -s * vx + c * vw;
    vx = nx;
    vw = nw;
}

inline void rotZW(float& vz, float& vw, float c, float s) {
    float nz = c * vz + s * vw;
    float nw = -s * vz + c * vw;
    vz = nz;
    vw = nw;
}

inline void rotYZ(float& vy, float& vz, float c, float s) {
    float ny = c * vy - s * vz;
    float nz = s * vy + c * vz;
    vy = ny;
    vz = nz;
}

inline void rotYW(float& vy, float& vw, float c, float s) {
    float ny = c * vy + s * vw;
    float nw = -s * vy + c * vw;
    vy = ny;
    vw = nw;
}

// W-perspective projection: applies focal-length-based perspective along W axis
// Input: 4D vertex (vx, vy, vz, vw) where camera looks along -W
// Output: 3D projected position
inline glm::vec3 project4Dto3D(float vx, float vy, float vz, float vw, float focalLength) {
    float negVw = -vw;
    if (negVw < 1e-4f) negVw = 1e-4f;  // clamp vertices at/behind camera
    float f = 1.0f / (negVw + focalLength);
    return glm::vec3(f * vx, f * vy, f * vz);
}

}  // namespace Math4D
