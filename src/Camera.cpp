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
      yawW(0.0f),
      yawZ(0.0f),
      pitch(0.0f),
      speed(1.5f),
      lookSpeed(60.0f) {}

Camera4D::Angles Camera4D::computeAngles() const {
    float cxwRad = glm::radians(yawW);
    float czwRad = glm::radians(yawZ);
    float cywRad = glm::radians(pitch);

    return Angles{
        cos(cxwRad), sin(cxwRad),
        cos(czwRad), sin(czwRad),
        cos(cywRad), sin(cywRad)
    };
}

void Camera4D::processInput(GLFWwindow* window, float dt, PhysicsBody& playerBody, PhysicsWorld& physWorld) {
    // Rotation
    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
        yawZ -= lookSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        yawZ += lookSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
        yawW -= lookSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
        yawW += lookSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
        pitch += lookSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
        pitch -= lookSpeed * dt;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    float moveSpeed = speed * dt;

    // Compute 4D camera basis vectors in world space
    Angles a = computeAngles();
    float cxw = a.cxw, sxw = a.sxw;
    float czw = a.czw, szw = a.szw;

    // Camera-relative world directions (inverse of XW→ZW rotation applied to basis vectors)
    float fwdX = cxw, fwdW = sxw;                           // camera X (forward)
    float rgtX = -sxw * szw, rgtZ = czw, rgtW = cxw * szw;  // camera Z (strafe)
    float advX = -sxw * czw, advZ = -szw, advW = cxw * czw; // camera W (advance)

    glm::vec4 desiredMove(0.0f);

    // E/A: forward/back (camera X direction)
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        desiredMove.x += moveSpeed * fwdX;
        desiredMove.w += moveSpeed * fwdW;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        desiredMove.x -= moveSpeed * fwdX;
        desiredMove.w -= moveSpeed * fwdW;
    }

    // Q/D: strafe left/right (camera Z direction)
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        desiredMove.x -= moveSpeed * rgtX;
        desiredMove.z -= moveSpeed * rgtZ;
        desiredMove.w -= moveSpeed * rgtW;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        desiredMove.x += moveSpeed * rgtX;
        desiredMove.z += moveSpeed * rgtZ;
        desiredMove.w += moveSpeed * rgtW;
    }

    // W/S: advance/retreat (camera W direction)
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        desiredMove.x += moveSpeed * advX;
        desiredMove.z += moveSpeed * advZ;
        desiredMove.w += moveSpeed * advW;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        desiredMove.x -= moveSpeed * advX;
        desiredMove.z -= moveSpeed * advZ;
        desiredMove.w -= moveSpeed * advW;
    }

    // Physics step
    playerBody.pos = pos;
    physWorld.step(playerBody, desiredMove, dt);
    pos = playerBody.pos;
}
