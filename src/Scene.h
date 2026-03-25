#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "Camera.h"
#include "Tesseract.h"
#include "physics.h"

struct Instance4D {
    glm::vec4 pos;
    float rotXZ, rotXW, rotZW;
    glm::vec3 colorA, colorB;
};

std::vector<Instance4D> buildScene(PhysicsWorld& physWorld);

bool projectInstance(
    const Instance4D& inst,
    const glm::vec4& camPos,
    const Camera4D::Angles& angles,
    float focalLength,
    const Tesseract& tesseract,
    std::vector<float>& outVertData
);
