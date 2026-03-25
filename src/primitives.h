#pragma once

#include "Object4D.h"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

inline Object4D generateHypercube() {
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
                        // Found a square: i, k, j, l (cyclic order - k and l adjacent to i in different dimensions)
                        if (i < j && i < k && i < l) {
                            std::vector<int> face = {i, k, j, l};  // Proper cyclic order: i -> k -> j -> l -> i
                            hypercube.cells.push_back(face);
                        }
                        break;
                    }
                }
            }
        }
    }

    // No need to deduplicate - we track with i < j && i < k && i < l to ensure uniqueness

    // Convert quads to triangles (same as game): (v0,v1,v2) + (v0,v2,v3)
    for (const auto& quad : hypercube.cells) {
        if (quad.size() == 4) {
            hypercube.triangleIndices.push_back(quad[0]);
            hypercube.triangleIndices.push_back(quad[1]);
            hypercube.triangleIndices.push_back(quad[2]);

            hypercube.triangleIndices.push_back(quad[0]);
            hypercube.triangleIndices.push_back(quad[2]);
            hypercube.triangleIndices.push_back(quad[3]);
        }
    }

    return hypercube;
}

inline Object4D generateHypersphere() {
    Object4D hypersphere;
    hypersphere.name = "Hypersphere";

    // Generate points on a 3-sphere (unit hypersphere in 4D)
    // Using proper Hopf coordinates: S^3 -> S^2 x S^1
    // (w, x, y, z) = (cos(θ)cos(φ), cos(θ)sin(φ)cos(ψ), cos(θ)sin(φ)sin(ψ), sin(θ))
    int resolution = 5;  // 5x5x5 = 125 vertices
    float radius = 0.5f;

    int vertexIndex = 0;
    std::vector<std::vector<std::vector<int>>> grid(resolution, std::vector<std::vector<int>>(resolution, std::vector<int>(resolution)));

    for (int i = 0; i < resolution; i++) {
        for (int j = 0; j < resolution; j++) {
            for (int k = 0; k < resolution; k++) {
                float theta = (i / (float)(resolution - 1)) * M_PI - M_PI / 2.0f;  // -π/2 to π/2
                float phi = (j / (float)(resolution - 1)) * M_PI;                   // 0 to π
                float psi = (k / (float)(resolution - 1)) * 2.0f * M_PI;            // 0 to 2π

                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);
                float cosPhi = std::cos(phi);
                float sinPhi = std::sin(phi);
                float cosPsi = std::cos(psi);
                float sinPsi = std::sin(psi);

                float w = radius * cosTheta * cosPhi;
                float x = radius * cosTheta * sinPhi * cosPsi;
                float y = radius * cosTheta * sinPhi * sinPsi;
                float z = radius * sinTheta;

                hypersphere.vertices.push_back(glm::vec4(x, y, z, w));
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

    // Convert quads to triangles
    for (const auto& quad : hypersphere.cells) {
        if (quad.size() == 4) {
            hypersphere.triangleIndices.push_back(quad[0]);
            hypersphere.triangleIndices.push_back(quad[1]);
            hypersphere.triangleIndices.push_back(quad[2]);

            hypersphere.triangleIndices.push_back(quad[0]);
            hypersphere.triangleIndices.push_back(quad[2]);
            hypersphere.triangleIndices.push_back(quad[3]);
        }
    }

    return hypersphere;
}
