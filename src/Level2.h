#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "Object4D.h"
#include "Scene.h"
#include "Math4D.h"
#include "physics.h"

struct ObjectInstance {
    glm::vec4 pos;
    Math4D::Rotor4D orientation;
    glm::vec3 colorA, colorB;
};

struct Level2Scene {
    std::vector<Instance4D> ground;
    std::vector<ObjectInstance> trees;
};

Level2Scene buildLevel2(PhysicsWorld& physWorld, const Object4D& treeObj);

bool projectObjectInstance(
    const Object4D& obj,
    const ObjectInstance& inst,
    const glm::vec4& camPos,
    const Math4D::Rotor4D& camOrientation,
    float focalLength,
    std::vector<float>& outVertData
);
