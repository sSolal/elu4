#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "physics.h"

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
    struct Angles {
        float cxw, sxw, czw, szw, cyw, syw;
    };

    glm::vec4 pos;
    float yawW;    // XW plane rotation (J/O keys)
    float yawZ;    // ZW plane rotation (U/L keys)
    float pitch;   // YW plane rotation (I/K keys)
    float speed;
    float lookSpeed;

    Camera4D();
    Angles computeAngles() const;
    void processInput(GLFWwindow* window, float dt, PhysicsBody& playerBody, PhysicsWorld& physWorld);
};
