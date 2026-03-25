#pragma once

#include <glm/glm.hpp>
#include <cmath>

namespace Math4D {

// Inline 4D rotation functions: mutate a vec4 in-place using pre-computed cos/sin
// These apply rotations in the specified planes using the angle representation.

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
