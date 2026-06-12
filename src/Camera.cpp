#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

Camera3D::Camera3D()
    : pos(-3.0f, 3.0f, 3.0f),
      up(0.0f, 1.0f, 0.0f),
      yaw(-45.0f),
      pitch(-35.0f),
      speed(1.5f),
      lookSpeed(60.0f) {}

glm::vec3 Camera3D::getFront() const {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    return glm::normalize(front);
}

glm::mat4 Camera3D::getViewMatrix() const {
    return glm::lookAt(pos, pos + getFront(), up);
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

void Camera4D::processInput(GLFWwindow* window, float dt, PhysicsBody& playerBody, PhysicsWorld& physWorld) {
    // Horizontal turning (U/L, J/O): compose into `yaw` within {X,Z,W} only, in
    // the local frame. These planes never involve Y, so the horizon can't tilt.
    float a = glm::radians(lookSpeed * dt);  // per-frame turn increment

    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
        yaw = Math4D::Rotor4D::fromZW(-a) * yaw;
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        yaw = Math4D::Rotor4D::fromZW(a) * yaw;
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
        yaw = Math4D::Rotor4D::fromXW(-a) * yaw;
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
        yaw = Math4D::Rotor4D::fromXW(a) * yaw;
    yaw.normalize();  // keep it a unit rotor across a long session

    // Look up/down (I/K): a single clamped pitch angle, applied OUTSIDE the yaw
    // in getOrientation(), so world-up stays in the camera Y-W plane (no roll).
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
        pitch += lookSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
        pitch -= lookSpeed * dt;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

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

    // Physics step
    playerBody.pos = pos;
    physWorld.step(playerBody, desiredMove, dt);
    pos = playerBody.pos;
}
