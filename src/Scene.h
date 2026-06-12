#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "Math4D.h"
#include "Tesseract.h"
#include "physics.h"

struct Instance4D {
    glm::vec4 pos;
    Math4D::Rotor4D orientation;
    glm::vec3 colorA, colorB;
};

std::vector<Instance4D> buildScene(PhysicsWorld& physWorld);

bool projectInstance(
    const Instance4D& inst,
    const glm::vec4& camPos,
    const Math4D::Rotor4D& camOrientation,
    float focalLength,
    const Tesseract& tesseract,
    std::vector<float>& outVertData
);
