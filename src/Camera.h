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

    Camera4D();
    Math4D::Rotor4D getOrientation() const {
        return Math4D::Rotor4D::fromYW(glm::radians(pitch)) * yaw;
    }
    void processInput(GLFWwindow* window, float dt, PhysicsBody& playerBody, PhysicsWorld& physWorld,
                      const LevelControls& ctrl = LevelControls{});
};
