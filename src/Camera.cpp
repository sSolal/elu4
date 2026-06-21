#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

Camera3D::Camera3D()
    : pos(-1.112f, 0.912f, 1.755f),
      up(0.0f, 1.0f, 0.0f),
      yaw(-57.30f),
      pitch(-26.11f),
      speed(1.5f),
      lookSpeed(60.0f) {}

glm::vec3 Camera3D::getFront() const {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    return glm::normalize(front);
}

namespace {
    // ---- Camera-sway tunables ----
    const glm::vec3 BOX_CENTER(0.0f);   // the 3D scene cube is centred at the origin
    const float SWAY_AMP         = 0.10f;  // angular swing amplitude (radians, ~6°)
    const float SWAY_PERIOD      = 6.0f;   // seconds per cycle (sine modes)
    const float SWAY_NOISE_SPEED = 0.15f;  // time scale for the random mode

    // Smooth 1-D value noise in [0,1] for the random camera-sway mode.
    float camHash(int n) {
        unsigned int x = (unsigned int)n * 374761393u + 1u;
        x = (x ^ (x >> 13)) * 1274126177u;
        return float((x ^ (x >> 16)) & 0xFFFFFFu) / float(0x1000000);
    }
    float camNoise(float t) {
        float fi = std::floor(t);
        int i = (int)fi;
        float f = t - fi;
        float a = camHash(i), b = camHash(i + 1);
        float u = f * f * (3.0f - 2.0f * f);
        return a + (b - a) * u;
    }
}

glm::mat4 Camera3D::getViewMatrix() const {
    glm::mat4 base = glm::lookAt(pos, pos + getFront(), up);
    if (oscMode == 0)
        return base;

    // Sway: rigidly rotate the WHOLE camera (position and orientation together)
    // about the box centre. The camera is not forced to look at the centre — it
    // keeps whatever it was aimed at, just swung around the centre in 3D — so the
    // scene rotates about the cube's middle rather than spinning in place.
    glm::vec3 rel = pos - BOX_CENTER;
    glm::vec3 axisUp = up;                                         // azimuth axis
    glm::vec3 axisRight = glm::normalize(glm::cross(axisUp, rel)); // elevation axis

    float w = 6.2831853f * oscTime / SWAY_PERIOD;
    float az = 0.0f, el = 0.0f;
    if (oscMode == 1) {                       // horizontal only
        az = SWAY_AMP * std::sin(w);
    } else if (oscMode == 2) {                // circle
        az = SWAY_AMP * std::sin(w);
        el = SWAY_AMP * std::cos(w);
    } else {                                  // random, centred on average
        az = SWAY_AMP * (2.0f * camNoise(oscTime * SWAY_NOISE_SPEED)         - 1.0f);
        el = SWAY_AMP * (2.0f * camNoise(oscTime * SWAY_NOISE_SPEED + 50.0f) - 1.0f);
    }

    // World-space rotation about the centre, applied rigidly to the camera pose:
    //   V_new = V_base * (T(C) · R · T(-C))^-1
    glm::mat4 R = glm::rotate(glm::mat4(1.0f), az, axisUp) *
                  glm::rotate(glm::mat4(1.0f), el, axisRight);
    glm::mat4 aboutCenter = glm::translate(glm::mat4(1.0f), BOX_CENTER) * R *
                            glm::translate(glm::mat4(1.0f), -BOX_CENTER);
    return base * glm::inverse(aboutCenter);
}

void Camera3D::processInput(GLFWwindow* window, float dt) {
    // Rotation
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
        yaw -= lookSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        yaw += lookSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
        pitch += lookSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
        pitch -= lookSpeed * dt;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    // Movement
    glm::vec3 front = getFront();
    glm::vec3 right = glm::normalize(glm::cross(front, up));
    float moveSpeed = speed * dt;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        pos += moveSpeed * front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        pos -= moveSpeed * front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        pos -= moveSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        pos += moveSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        pos += moveSpeed * up;
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        pos -= moveSpeed * up;
}

Camera4D::Camera4D()
    : pos(0.0f, 2.5f, 0.0f, 0.0f),
      yaw(Math4D::Rotor4D::identity()),
      pitch(0.0f),
      speed(1.5f),
      lookSpeed(60.0f) {}

void Camera4D::processInput(GLFWwindow* window, float dt, PhysicsBody& playerBody, PhysicsWorld& physWorld,
                            const LevelControls& ctrl) {
    // Horizontal turning (U/L, J/O): compose into `yaw` within {X,Z,W} only, in
    // the local frame. These planes never involve Y, so the horizon can't tilt.
    float a = glm::radians(lookSpeed * dt);  // per-frame turn increment

    bool lookInput = false;  // did the player actively turn the head this frame?
    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) {
        yaw = Math4D::Rotor4D::fromZW(-a) * yaw; lookInput = true;
    }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        yaw = Math4D::Rotor4D::fromZW(a) * yaw; lookInput = true;
    }
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) {
        yaw = Math4D::Rotor4D::fromXW(-a) * yaw; lookInput = true;
    }
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
        yaw = Math4D::Rotor4D::fromXW(a) * yaw; lookInput = true;
    }
    yaw.normalize();  // keep it a unit rotor across a long session

    // Look up/down (I/K): a single clamped pitch angle, applied OUTSIDE the yaw
    // in getOrientation(), so world-up stays in the camera Y-W plane (no roll).
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        pitch += lookSpeed * dt; lookInput = true;
    }
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
        pitch -= lookSpeed * dt; lookInput = true;
    }
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    // Gentle head-return: when the level asks for it and the player isn't
    // actively looking, ease yaw and pitch back to their defaults. Yaw and pitch
    // are sprung SEPARATELY (never the combined orientation) so the flat-horizon
    // guarantee — pitch applied outside yaw — is preserved and no roll creeps in.
    if (ctrl.headReturn && !lookInput) {
        float k = 1.0f - std::exp(-ctrl.headReturnStrength * dt);  // frame-rate independent

        yaw = Math4D::Rotor4D::nlerp(yaw, ctrl.defaultYaw, k);
        if (Math4D::Rotor4D::dot(yaw, ctrl.defaultYaw) > 0.99999f)
            yaw = ctrl.defaultYaw;
        yaw.normalize();

        pitch += (ctrl.defaultPitch - pitch) * k;
        if (std::abs(pitch - ctrl.defaultPitch) < ctrl.headReturnSnap)
            pitch = ctrl.defaultPitch;
    }

    float moveSpeed = speed * dt;

    // Compute 4D camera basis vectors in world space via inverse (reverse) of camera rotor
    Math4D::Rotor4D R = getOrientation();
    Math4D::Rotor4D Rrev = R.reverse();
    glm::mat4 invM = Rrev.toMatrix();
    glm::vec4 fwd = invM * glm::vec4(1,0,0,0);  // camera X in world (forward)
    glm::vec4 rgt = invM * glm::vec4(0,0,1,0);  // camera Z in world (strafe)
    glm::vec4 adv = invM * glm::vec4(0,0,0,1);  // camera W in world (advance)

    glm::vec4 desiredMove(0.0f);

    // E/A: forward/back (camera X direction)
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) desiredMove += moveSpeed * fwd;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) desiredMove -= moveSpeed * fwd;

    // Q/D: strafe left/right (camera Z direction)
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) desiredMove -= moveSpeed * rgt;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) desiredMove += moveSpeed * rgt;

    // W/S: advance/retreat (camera W direction)
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) desiredMove += moveSpeed * adv;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) desiredMove -= moveSpeed * adv;

    // Axis lock: forbid translation along the level's locked axes by masking the
    // desired move before the physics step. World-space locking just zeroes the
    // world components; camera-space removes the locked camera-basis projections.
    // (Note: physics overwrites .y with gravity, so locking Y here is a no-op
    //  against falling — fine for levels like the Corridor that lock only X/Z.)
    if (ctrl.lockedAxes != AxisLock::NONE) {
        if (ctrl.lockIsWorldSpace) {
            if (ctrl.lockedAxes & AxisLock::X) desiredMove.x = 0.0f;
            if (ctrl.lockedAxes & AxisLock::Y) desiredMove.y = 0.0f;
            if (ctrl.lockedAxes & AxisLock::Z) desiredMove.z = 0.0f;
            if (ctrl.lockedAxes & AxisLock::W) desiredMove.w = 0.0f;
        } else {
            // Camera-space: subtract the component along each locked camera axis.
            if (ctrl.lockedAxes & AxisLock::X) desiredMove -= glm::dot(desiredMove, fwd) * fwd;
            if (ctrl.lockedAxes & AxisLock::Z) desiredMove -= glm::dot(desiredMove, rgt) * rgt;
            if (ctrl.lockedAxes & AxisLock::W) desiredMove -= glm::dot(desiredMove, adv) * adv;
        }
    }

    // Physics step
    playerBody.pos = pos;
    physWorld.step(playerBody, desiredMove, dt);
    pos = playerBody.pos;
}
