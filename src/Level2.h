#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "Object4D.h"
#include "Scene.h"        // ObjectInstance
#include "Math4D.h"

// Project one placed object into the interleaved vertex buffer the inner shader
// consumes (7 floats/vertex: xyz projected + rgb + camera-relative 4D depth).
// Returns false when the whole instance is behind the 4D camera. This is THE
// projection path for every mesh in the game.
bool projectObjectInstance(
    const Object4D& obj,
    const ObjectInstance& inst,
    const glm::vec4& camPos,
    const Math4D::Rotor4D& camOrientation,
    float focalLength,
    std::vector<float>& outVertData
);
