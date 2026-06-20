#pragma once

#include <glm/glm.hpp>
#include "Math4D.h"

// A placed 4D object: world position, 4D orientation (geometric-algebra rotor), and
// a two-tone colour that grades along the object's local W (see projectObjectInstance
// in Level2.cpp). One mesh + many ObjectInstances is the universal placement unit for
// every level.
struct ObjectInstance {
    glm::vec4 pos;
    Math4D::Rotor4D orientation;
    glm::vec3 colorA, colorB;
};
