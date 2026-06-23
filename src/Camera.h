#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "physics.h"
#include "Math4D.h"
#include "LevelControls.h"

class Camera3D {
public:
    glm::vec3 pos;
    glm::vec3 up;
    float yaw;
    float pitch;
    float speed;
    float lookSpeed;

    // Observer-camera oscillation (key M), set by the runner each frame from
    // RenderSettings. The look target stays fixed while the eye sways, producing
    // motion parallax — a strong kinetic depth cue. 0=static, 1=circular, 2=forward.
    int   oscMode = 0;
    float oscTime = 0.0f;

    Camera3D();
    glm::vec3 getFront() const;
    glm::mat4 getViewMatrix() const;
    void processInput(GLFWwindow* window, float dt);
};

class Camera4D {
public:
    glm::vec4 pos;
    // Flat-horizon FPS orientation, split so the world "up" (+Y) can never roll:
    //   * yaw   — heading within the horizontal space {X,Z,W}; never touches Y
    //             (its bivectors stay in {XW,ZW,XZ}, which is closed under the
    //              geometric product, so no Y component is ever introduced)
    //   * pitch — look up/down in the Y-W plane, applied OUTSIDE the yaw
    // getOrientation() = fromYW(pitch) * yaw therefore maps world-up Y into the
    // camera Y-W plane only (zero X/Z leak), so the horizon stays level always.
    Math4D::Rotor4D yaw;
    float pitch;      // degrees, look up/down, clamped to +/-89
    float speed;
    float lookSpeed;  // degrees per second

    // Soft-lock excursions (degrees). When a level marks a turn plane as locked, the
    // keys still nudge the head into that plane (slowing as they near softLockMax) and
    // spring back to zero on release — so the player sees the key "does something" but
    // isn't free yet. These are temporary, never bake into the committed yaw/pitch:
    // they fold into the *view* (getOrientation) only; movement uses the committed pose.
    float softXW = 0.0f;
    float softZW = 0.0f;
    float softPitch = 0.0f;

    Camera4D();
    // The committed pose, excluding the springy soft-lock excursion (used for movement).
    Math4D::Rotor4D getCommittedOrientation() const {
        return Math4D::Rotor4D::fromYW(glm::radians(pitch)) * yaw;
    }
    // The viewed pose: committed pose plus any soft-lock head excursion.
    Math4D::Rotor4D getOrientation() const {
        Math4D::Rotor4D y = yaw;
        if (softXW != 0.0f) y = Math4D::Rotor4D::fromXW(glm::radians(softXW)) * y;
        if (softZW != 0.0f) y = Math4D::Rotor4D::fromZW(glm::radians(softZW)) * y;
        return Math4D::Rotor4D::fromYW(glm::radians(pitch + softPitch)) * y;
    }
    void processInput(GLFWwindow* window, float dt, PhysicsBody& playerBody, PhysicsWorld& physWorld,
                      const LevelControls& ctrl = LevelControls{});
};
