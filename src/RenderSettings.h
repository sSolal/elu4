#pragma once

#include <glm/glm.hpp>

// Global, live-toggleable visualization aids for reading the projected-4D scene.
// Owned by the runner (main.cpp) and threaded through LevelContext so every level
// and both view modes (3D observer / 4D FPS) share one set of switches.
//
// The "depth" driving fog / transparency / line-thickness is the camera-relative
// 4D distance d = -v.w, computed per vertex in the projection functions and passed
// to the inner shader as a vertex attribute.

enum class GeomMode  { Solid = 0, Wireframe, SolidBorders };          // P
enum class CamOsc    { Static = 0, Horizontal, Circle, Random };     // M
enum class DepthCue  { Fog = 0, Normal, DarkenFar };                 // N
enum class AlphaMode { Mid = 0, Light, Heavy, NearTransparent };     // F
enum class PulseMode { Off = 0, Sine, Noise };                       // V

struct RenderSettings {
    GeomMode  geom  = GeomMode::SolidBorders;   // default: solid with borders
    CamOsc    osc   = CamOsc::Static;
    DepthCue  depth = DepthCue::Fog;            // default: depth fog
    AlphaMode alpha = AlphaMode::Mid;
    PulseMode pulse = PulseMode::Off;
    int       bg    = 0;                        // default: warm off-white

    float time = 0.0f;            // accumulated seconds (pulse + camera sway)

    // ---- Tunable look constants (central place to tweak the visual feel) ----
    // Depth normalisation window (world units along W) for fog / near-transparent
    // / line-thickness. Smaller depthFar = fog/cues ramp in sooner.
    float depthNear = 0.5f;
    float depthFar  = 10.0f;
    // How hard the distance fog pulls far objects toward the sky colour (0..~1.2).
    float fogStrength = 1.0f;
    // Global vibrance: push every base colour away from grey so the depth fog
    // reads clearly even on muted scene colours (1 = no change).
    float vibrance = 1.4f;

    // The three background / sky colours B cycles through. The empty space in the
    // scene is this colour, and the distance fog blends far objects into it.
    glm::vec3 bgColor() const {
        switch (bg) {
            case 0:  return glm::vec3(0.55f, 0.72f, 0.90f);  // light sky blue
            case 1:  return glm::vec3(0.04f, 0.07f, 0.16f);  // deep blue (dusk)
            default: return glm::vec3(0.0f,  0.0f,  0.0f);   // black (night)
        }
    }

    // Borders are the solid's own colour nudged toward this target by borderAmt(),
    // so they read as "the same surface, just outlined": darker on a light
    // background, lighter on a dark one.
    glm::vec3 borderTarget() const {
        return bg == 0 ? glm::vec3(0.0f) : glm::vec3(1.0f);
    }
    float borderAmt() const {
        return bg == 0 ? 0.45f : 0.5f;
    }
};
