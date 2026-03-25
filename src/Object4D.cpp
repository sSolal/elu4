#include "Object4D.h"
#include <fstream>
#include <sstream>
#include <cmath>

Object4D::Object4D(const std::string& filePath) {
    loadFromJSON(filePath);
}

bool Object4D::loadFromJSON(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Simple JSON parsing - look for "name", "vertices", "edges", "cells"
    auto extractString = [&content](const std::string& key) -> std::string {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = content.find(":", pos);
        if (pos == std::string::npos) return "";
        pos = content.find("\"", pos);
        if (pos == std::string::npos) return "";
        pos++;
        size_t end = content.find("\"", pos);
        if (end == std::string::npos) return "";
        return content.substr(pos, end - pos);
    };

    auto extractArray = [&content](const std::string& key) -> std::string {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = content.find("[", pos);
        if (pos == std::string::npos) return "";
        int depth = 1;
        size_t start = pos + 1;
        pos++;
        while (pos < content.size() && depth > 0) {
            if (content[pos] == '[') depth++;
            else if (content[pos] == ']') depth--;
            pos++;
        }
        return content.substr(start, pos - start - 1);
    };

    name = extractString("name");

    // Parse vertices
    std::string vertArray = extractArray("vertices");
    std::stringstream vertStream(vertArray);
    std::string line;
    while (std::getline(vertStream, line, ']')) {
        size_t start = line.find('[');
        if (start != std::string::npos) {
            line = line.substr(start + 1);
            std::stringstream coords(line);
            float x, y, z, w;
            char comma;
            if (coords >> x >> comma >> y >> comma >> z >> comma >> w) {
                vertices.push_back(glm::vec4(x, y, z, w));
            }
        }
    }

    // Parse edges
    std::string edgeArray = extractArray("edges");
    std::stringstream edgeStream(edgeArray);
    while (std::getline(edgeStream, line, ']')) {
        size_t start = line.find('[');
        if (start != std::string::npos) {
            line = line.substr(start + 1);
            std::stringstream indices(line);
            int i, j;
            char comma;
            if (indices >> i >> comma >> j) {
                edges.push_back({i, j});
            }
        }
    }

    // Parse cells
    std::string cellArray = extractArray("cells");
    std::stringstream cellStream(cellArray);
    while (std::getline(cellStream, line, ']')) {
        size_t start = line.find('[');
        if (start != std::string::npos) {
            line = line.substr(start + 1);
            std::vector<int> cell;
            std::stringstream indices(line);
            int idx;
            char comma;
            while (indices >> idx) {
                cell.push_back(idx);
                indices >> comma;
            }
            if (!cell.empty()) {
                cells.push_back(cell);
            }
        }
    }

    return !vertices.empty();
}

bool Object4D::saveToJSON(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filePath << std::endl;
        return false;
    }

    file << "{\n";
    file << "  \"name\": \"" << name << "\",\n";

    // Write vertices
    file << "  \"vertices\": [\n";
    for (size_t i = 0; i < vertices.size(); i++) {
        file << "    [" << vertices[i].x << ", " << vertices[i].y << ", "
             << vertices[i].z << ", " << vertices[i].w << "]";
        if (i < vertices.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // Write edges
    file << "  \"edges\": [\n";
    for (size_t i = 0; i < edges.size(); i++) {
        file << "    [" << edges[i].first << ", " << edges[i].second << "]";
        if (i < edges.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // Write cells
    file << "  \"cells\": [\n";
    for (size_t i = 0; i < cells.size(); i++) {
        file << "    [";
        for (size_t j = 0; j < cells[i].size(); j++) {
            file << cells[i][j];
            if (j < cells[i].size() - 1) file << ", ";
        }
        file << "]";
        if (i < cells.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";

    file.close();
    return true;
}
