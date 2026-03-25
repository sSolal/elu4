#include "primitives.h"
#include "Object4D.h"
#include "Math4D.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>

void printUsage() {
    std::cerr << "Usage:\n"
              << "  4dg put <hypercube|hypersphere> <output.json>\n"
              << "  4dg stretch <file.json> <sx> <sy> <sz> <sw>\n"
              << "  4dg translate <file.json> <dx> <dy> <dz> <dw>\n"
              << "  4dg rotate <file.json> <plane> <angle_deg>\n"
              << "  4dg add <src.json> <dst.json> [dx dy dz dw]\n"
              << "  4dg clear <file.json>\n\n"
              << "Planes: xy, xz, xw, yz, yw, zw\n";
}

float deg2rad(float deg) {
    return deg * (M_PI / 180.0f);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string cmd = argv[1];

    try {
        if (cmd == "put") {
            if (argc != 4) {
                std::cerr << "put: expected 2 arguments\n";
                return 1;
            }
            std::string primitive = argv[2];
            std::string output = argv[3];

            Object4D obj;
            if (primitive == "hypercube") {
                obj = generateHypercube();
            } else if (primitive == "hypersphere") {
                obj = generateHypersphere();
            } else {
                std::cerr << "Unknown primitive: " << primitive << "\n";
                return 1;
            }

            obj.saveToJSON(output);
            std::cout << "Created " << output << "\n";
            return 0;
        }

        if (cmd == "stretch") {
            if (argc != 7) {
                std::cerr << "stretch: expected 5 arguments\n";
                return 1;
            }
            std::string file = argv[2];
            float sx = std::stof(argv[3]);
            float sy = std::stof(argv[4]);
            float sz = std::stof(argv[5]);
            float sw = std::stof(argv[6]);

            Object4D obj(file);
            for (auto& v : obj.vertices) {
                v *= glm::vec4(sx, sy, sz, sw);
            }
            obj.saveToJSON(file);
            std::cout << "Stretched " << file << "\n";
            return 0;
        }

        if (cmd == "translate") {
            if (argc != 7) {
                std::cerr << "translate: expected 5 arguments\n";
                return 1;
            }
            std::string file = argv[2];
            float dx = std::stof(argv[3]);
            float dy = std::stof(argv[4]);
            float dz = std::stof(argv[5]);
            float dw = std::stof(argv[6]);

            Object4D obj(file);
            for (auto& v : obj.vertices) {
                v += glm::vec4(dx, dy, dz, dw);
            }
            obj.saveToJSON(file);
            std::cout << "Translated " << file << "\n";
            return 0;
        }

        if (cmd == "rotate") {
            if (argc != 5) {
                std::cerr << "rotate: expected 3 arguments\n";
                return 1;
            }
            std::string file = argv[2];
            std::string plane = argv[3];
            float angle = deg2rad(std::stof(argv[4]));

            float c = std::cos(angle);
            float s = std::sin(angle);

            Object4D obj(file);

            for (auto& v : obj.vertices) {
                if (plane == "xy") {
                    Math4D::rotXY(v.x, v.y, c, s);
                } else if (plane == "xz") {
                    Math4D::rotXZ(v.x, v.z, c, s);
                } else if (plane == "xw") {
                    Math4D::rotXW(v.x, v.w, c, s);
                } else if (plane == "yz") {
                    Math4D::rotYZ(v.y, v.z, c, s);
                } else if (plane == "yw") {
                    Math4D::rotYW(v.y, v.w, c, s);
                } else if (plane == "zw") {
                    Math4D::rotZW(v.z, v.w, c, s);
                } else {
                    std::cerr << "Unknown plane: " << plane << "\n";
                    return 1;
                }
            }

            obj.saveToJSON(file);
            std::cout << "Rotated " << file << " in plane " << plane << "\n";
            return 0;
        }

        if (cmd == "add") {
            if (argc < 4 || argc > 8) {
                std::cerr << "add: expected 2-5 arguments\n";
                return 1;
            }
            std::string src_file = argv[2];
            std::string dst_file = argv[3];

            float dx = 0.0f, dy = 0.0f, dz = 0.0f, dw = 0.0f;
            if (argc >= 5) dx = std::stof(argv[4]);
            if (argc >= 6) dy = std::stof(argv[5]);
            if (argc >= 7) dz = std::stof(argv[6]);
            if (argc >= 8) dw = std::stof(argv[7]);

            Object4D src(src_file);
            Object4D dst;
            // Try to load dst file, but create empty if it doesn't exist
            std::ifstream test_file(dst_file);
            if (test_file.good()) {
                test_file.close();
                dst = Object4D(dst_file);
            } else {
                test_file.close();
                dst = Object4D();
            }

            // Optionally translate src
            if (dx != 0.0f || dy != 0.0f || dz != 0.0f || dw != 0.0f) {
                glm::vec4 offset(dx, dy, dz, dw);
                for (auto& v : src.vertices) {
                    v += offset;
                }
            }

            // Merge src into dst
            unsigned int vertex_offset = dst.vertices.size();
            dst.vertices.insert(dst.vertices.end(), src.vertices.begin(), src.vertices.end());

            for (const auto& edge : src.edges) {
                dst.edges.push_back({edge.first + vertex_offset, edge.second + vertex_offset});
            }

            for (const auto& cell : src.cells) {
                std::vector<int> shifted_cell;
                for (int idx : cell) {
                    shifted_cell.push_back(idx + vertex_offset);
                }
                dst.cells.push_back(shifted_cell);
            }

            // Re-triangulate cells for rendering
            dst.triangleIndices.clear();
            for (const auto& quad : dst.cells) {
                if (quad.size() == 4) {
                    dst.triangleIndices.push_back(quad[0]);
                    dst.triangleIndices.push_back(quad[1]);
                    dst.triangleIndices.push_back(quad[2]);

                    dst.triangleIndices.push_back(quad[0]);
                    dst.triangleIndices.push_back(quad[2]);
                    dst.triangleIndices.push_back(quad[3]);
                }
            }

            dst.saveToJSON(dst_file);
            std::cout << "Merged " << src_file << " into " << dst_file << "\n";
            return 0;
        }

        if (cmd == "clear") {
            if (argc != 3) {
                std::cerr << "clear: expected 1 argument\n";
                return 1;
            }
            std::string file = argv[2];

            if (std::remove(file.c_str()) != 0) {
                std::cerr << "Warning: failed to delete " << file << "\n";
            }
            std::cout << "Deleted " << file << "\n";
            return 0;
        }

        std::cerr << "Unknown command: " << cmd << "\n";
        printUsage();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
