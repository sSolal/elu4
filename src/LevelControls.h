#pragma once

#include "Math4D.h"

// Per-level control configuration. Plain data, owned by each Level and threaded
// into Camera4D::processInput. Two mechanics live here:
//
//   * Axis lock     — forbid translation along chosen axes (e.g. the Corridor
//                     locks the X/Z strafe plane so you can only move along W).
//   * Head return   — when the player turns the head but the level wants it to
//                     spring back to a default facing (Corridor, Dodgeball).
//
// A `mode` selects the control scheme; only FPS is implemented this pass.

namespace AxisLock {
    // Bitmask of axes to forbid. Combine with '|'.
    enum : unsigned { NONE = 0, X = 1, Y = 2, Z = 4, W = 8 };
}

struct LevelControls {
    // --- Axis lock (translation) ---
    unsigned lockedAxes      = AxisLock::NONE;  // axes to zero out of desiredMove
    bool     lockIsWorldSpace = true;           // true = world axes, false = camera axes

    // --- Head return (orientation spring) ---
    bool            headReturn         = false;  // spring yaw/pitch toward defaults
    float           headReturnStrength = 8.0f;   // 1/sec; higher = snappier
    float           headReturnSnap     = 0.5f;   // degrees: snap pitch to default below this
    Math4D::Rotor4D defaultYaw         = Math4D::Rotor4D::identity();
    float           defaultPitch       = 0.0f;   // degrees

    // --- Control scheme (only FPS implemented; others are stubs) ---
    enum class Mode { FPS, ORBIT, FLIGHT } mode = Mode::FPS;
};
