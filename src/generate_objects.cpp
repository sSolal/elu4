#include "Object4D.h"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

void generateHypercube() {
    Object4D hypercube;
    hypercube.name = "Hypercube";

    // Generate all 16 vertices (2^4 combinations of ±0.5)
    for (int i = 0; i < 16; i++) {
        float x = ((i & 1) ? 0.5f : -0.5f);
        float y = ((i & 2) ? 0.5f : -0.5f);
        float z = ((i & 4) ? 0.5f : -0.5f);
        float w = ((i & 8) ? 0.5f : -0.5f);
        hypercube.vertices.push_back(glm::vec4(x, y, z, w));
    }

    // Generate edges - connect vertices that differ in exactly one coordinate
    for (int i = 0; i < 16; i++) {
        for (int j = i + 1; j < 16; j++) {
            int diff = i ^ j;
            // Check if only one bit differs
            if ((diff & (diff - 1)) == 0) {
                hypercube.edges.push_back({i, j});
            }
        }
    }

    // Generate cells (2D faces) - connect vertices that differ in exactly two coordinates
    for (int i = 0; i < 16; i++) {
        for (int j = i + 1; j < 16; j++) {
            int diff = i ^ j;
            int bitCount = __builtin_popcount(diff);
            if (bitCount == 2) {
                // Found a pair, look for the other two vertices of the square
                for (int k = 0; k < 16; k++) {
                    if (k == i || k == j) continue;
                    int d1 = i ^ k;
                    int d2 = j ^ k;
                    if (__builtin_popcount(d1) == 1 && __builtin_popcount(d2) == 1) {
                        int l = i ^ j ^ k;
                        // Found a square: i, j, k, l
                        if (i < j && i < k && i < l) {
                            std::vector<int> face = {i, j, k, l};
                            std::sort(face.begin(), face.end());
                            hypercube.cells.push_back(face);
                        }
                        break;
                    }
                }
            }
        }
    }

    // Remove duplicates from cells
    std::sort(hypercube.cells.begin(), hypercube.cells.end());
    hypercube.cells.erase(std::unique(hypercube.cells.begin(), hypercube.cells.end()), hypercube.cells.end());

    hypercube.saveToJSON("hypercube.json");
    std::cout << "Generated hypercube.json with " << hypercube.vertices.size() << " vertices, "
              << hypercube.edges.size() << " edges, and " << hypercube.cells.size() << " cells" << std::endl;
}

void generateHypersphere() {
    Object4D hypersphere;
    hypersphere.name = "Hypersphere";

    // Generate points on a 3-sphere (unit hypersphere in 4D)
    // Using parametrization with 3 angles (similar to spherical coords)
    int resolution = 4;  // Low resolution: 4x4x4 = 64 vertices
    float radius = 0.5f;

    int vertexIndex = 0;
    std::vector<std::vector<std::vector<int>>> grid(resolution, std::vector<std::vector<int>>(resolution, std::vector<int>(resolution)));

    for (int i = 0; i < resolution; i++) {
        for (int j = 0; j < resolution; j++) {
            for (int k = 0; k < resolution; k++) {
                float u = (i / (float)(resolution - 1)) * M_PI;      // 0 to pi
                float v = (j / (float)(resolution - 1)) * 2.0f * M_PI;  // 0 to 2pi
                float w = (k / (float)(resolution - 1)) * 2.0f * M_PI;  // 0 to 2pi

                float x = radius * std::sin(u) * std::cos(v);
                float y = radius * std::sin(u) * std::sin(v);
                float z = radius * std::cos(u) * std::cos(w);
                float wc = radius * std::cos(u) * std::sin(w);

                hypersphere.vertices.push_back(glm::vec4(x, y, z, wc));
                grid[i][j][k] = vertexIndex++;
            }
        }
    }

    // Connect nearby vertices with edges
    for (int i = 0; i < resolution; i++) {
        for (int j = 0; j < resolution; j++) {
            for (int k = 0; k < resolution; k++) {
                // Connect to next in each dimension
                if (i < resolution - 1) {
                    hypersphere.edges.push_back({grid[i][j][k], grid[i + 1][j][k]});
                }
                if (j < resolution - 1) {
                    hypersphere.edges.push_back({grid[i][j][k], grid[i][j + 1][k]});
                }
                if (k < resolution - 1) {
                    hypersphere.edges.push_back({grid[i][j][k], grid[i][j][k + 1]});
                }
            }
        }
    }

    // Generate cell faces (quads)
    for (int i = 0; i < resolution - 1; i++) {
        for (int j = 0; j < resolution - 1; j++) {
            for (int k = 0; k < resolution - 1; k++) {
                // XY face
                hypersphere.cells.push_back({grid[i][j][k], grid[i + 1][j][k], grid[i + 1][j + 1][k], grid[i][j + 1][k]});
                // XZ face
                hypersphere.cells.push_back({grid[i][j][k], grid[i + 1][j][k], grid[i + 1][j][k + 1], grid[i][j][k + 1]});
                // YZ face
                hypersphere.cells.push_back({grid[i][j][k], grid[i][j + 1][k], grid[i][j + 1][k + 1], grid[i][j][k + 1]});
            }
        }
    }

    hypersphere.saveToJSON("hypersphere.json");
    std::cout << "Generated hypersphere.json with " << hypersphere.vertices.size() << " vertices, "
              << hypersphere.edges.size() << " edges, and " << hypersphere.cells.size() << " cells" << std::endl;
}

int main() {
    generateHypercube();
    generateHypersphere();
    return 0;
}
