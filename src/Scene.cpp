#include "Scene.h"
#include "Math4D.h"
#include <cmath>

std::vector<Instance4D> buildScene(PhysicsWorld& physWorld) {
    std::vector<Instance4D> instances;

    glm::vec3 groundGray(0.5f, 0.5f, 0.5f);

    // Ground grid: hypercubes at y = -HS (below ground level), 9x9x9
    for (float x = -2.4f; x <= 2.4f; x += 0.6f) {
        for (float z = -2.4f; z <= 2.4f; z += 0.6f) {
            for (float w = -2.4f; w <= 2.4f; w += 0.6f) {
                instances.push_back({
                    {x, -Tesseract::HS, z, w},
                    0.0f, 0.0f, 0.0f,
                    groundGray, groundGray
                });
                physWorld.addObject({x, -Tesseract::HS, z, w}, Tesseract::HS);
            }
        }
    }

    // Hypercubes floating above ground
    instances.push_back({
        {0.0f, Tesseract::HS, 0.0f, 0.0f},
        0.0f, 0.0f, 0.0f,
        glm::vec3(0.2f, 0.4f, 1.0f),
        glm::vec3(1.0f, 0.8f, 0.2f)
    });
    physWorld.addObject({0.0f, Tesseract::HS, 0.0f, 0.0f}, Tesseract::HS);

    instances.push_back({
        {1.8f, Tesseract::HS, 1.2f, 0.8f},
        0.52f, 0.0f, 0.31f,
        glm::vec3(1.0f, 0.2f, 0.2f),
        glm::vec3(1.0f, 1.0f, 0.2f)
    });
    physWorld.addObject({1.8f, Tesseract::HS, 1.2f, 0.8f}, Tesseract::HS);

    instances.push_back({
        {-1.8f, Tesseract::HS, 1.5f, -1.0f},
        1.1f, 0.0f, 0.0f,
        glm::vec3(0.2f, 1.0f, 0.2f),
        glm::vec3(0.8f, 0.2f, 1.0f)
    });
    physWorld.addObject({-1.8f, Tesseract::HS, 1.5f, -1.0f}, Tesseract::HS);

    instances.push_back({
        {1.5f, Tesseract::HS, -1.8f, 1.2f},
        0.0f, 0.78f, 0.0f,
        glm::vec3(1.0f, 0.5f, 0.2f),
        glm::vec3(0.2f, 1.0f, 1.0f)
    });
    physWorld.addObject({1.5f, Tesseract::HS, -1.8f, 1.2f}, Tesseract::HS);

    instances.push_back({
        {-1.5f, Tesseract::HS, -1.2f, 0.9f},
        0.31f, 0.52f, 0.0f,
        glm::vec3(0.8f, 0.2f, 0.8f),
        glm::vec3(0.2f, 1.0f, 0.5f)
    });
    physWorld.addObject({-1.5f, Tesseract::HS, -1.2f, 0.9f}, Tesseract::HS);

    return instances;
}

bool projectInstance(
    const Instance4D& inst,
    const glm::vec4& camPos,
    const Camera4D::Angles& angles,
    float focalLength,
    const Tesseract& tesseract,
    std::vector<float>& outVertData
) {
    // Check if instance is culled (entirely behind camera in W)
    bool behindCamera = true;
    float cxw = angles.cxw, sxw = angles.sxw;
    float czw = angles.czw, szw = angles.szw;
    float cyw = angles.cyw, syw = angles.syw;

    for (int i = 0; i < 16; i++) {
        glm::vec4 localV = tesseract.verts4D[i];
        float vx = localV.x, vy = localV.y, vz = localV.z, vw = localV.w;

        // Local rotations (XZ, XW, ZW)
        Math4D::rotXZ(vx, vz, cos(inst.rotXZ), sin(inst.rotXZ));
        Math4D::rotXW(vx, vw, cos(inst.rotXW), sin(inst.rotXW));
        Math4D::rotZW(vz, vw, cos(inst.rotZW), sin(inst.rotZW));

        // Translate + camera space
        vx += inst.pos.x - camPos.x;
        vy += inst.pos.y - camPos.y;
        vz += inst.pos.z - camPos.z;
        vw += inst.pos.w - camPos.w;

        // Camera rotations
        Math4D::rotXW(vx, vw, cxw, sxw);
        Math4D::rotZW(vz, vw, czw, szw);
        Math4D::rotYW(vy, vw, cyw, syw);

        if (-vw > 0) {
            behindCamera = false;
            break;
        }
    }

    if (behindCamera) {
        return false;  // culled
    }

    // Project all vertices
    outVertData.clear();
    outVertData.reserve(16 * 6);

    for (int i = 0; i < 16; i++) {
        glm::vec4 localV = tesseract.verts4D[i];
        float vx = localV.x, vy = localV.y, vz = localV.z, vw = localV.w;

        // Local rotations
        Math4D::rotXZ(vx, vz, cos(inst.rotXZ), sin(inst.rotXZ));
        Math4D::rotXW(vx, vw, cos(inst.rotXW), sin(inst.rotXW));
        Math4D::rotZW(vz, vw, cos(inst.rotZW), sin(inst.rotZW));

        // Translate + camera space
        vx += inst.pos.x - camPos.x;
        vy += inst.pos.y - camPos.y;
        vz += inst.pos.z - camPos.z;
        vw += inst.pos.w - camPos.w;

        // Camera rotations
        Math4D::rotXW(vx, vw, cxw, sxw);
        Math4D::rotZW(vz, vw, czw, szw);
        Math4D::rotYW(vy, vw, cyw, syw);

        // W-perspective projection
        glm::vec3 p = Math4D::project4Dto3D(vx, vy, vz, vw, focalLength);

        // Color based on W coordinate
        float t = (localV.w + Tesseract::HS) / (2.0f * Tesseract::HS);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        glm::vec3 c = glm::mix(inst.colorA, inst.colorB, t);

        outVertData.push_back(p.x);
        outVertData.push_back(p.y);
        outVertData.push_back(p.z);
        outVertData.push_back(c.r);
        outVertData.push_back(c.g);
        outVertData.push_back(c.b);
    }

    return true;  // not culled
}
