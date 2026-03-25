#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <iostream>

struct Object4D {
    std::string name;
    std::vector<glm::vec4> vertices;
    std::vector<std::pair<int, int>> edges;
    std::vector<std::vector<int>> cells;  // Each cell is a list of vertex indices (polygon in 4D)
    std::vector<unsigned int> triangleIndices;  // Pre-triangulated face indices for rendering

    Object4D() = default;
    Object4D(const std::string& filePath);

    bool loadFromJSON(const std::string& filePath);
    bool saveToJSON(const std::string& filePath) const;
};
