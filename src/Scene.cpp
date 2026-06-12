#include "Scene.h"
#include "Math4D.h"

std::vector<Instance4D> buildScene(PhysicsWorld& physWorld) {
    std::vector<Instance4D> instances;

    glm::vec3 groundGray(0.5f, 0.5f, 0.5f);

    // Ground grid: hypercubes at y = -HS (below ground level), 9x9x9
    for (float x = -2.4f; x <= 2.4f; x += 0.6f) {
        for (float z = -2.4f; z <= 2.4f; z += 0.6f) {
            for (float w = -2.4f; w <= 2.4f; w += 0.6f) {
                instances.push_back({
                    {x, -Tesseract::HS, z, w},
                    Math4D::Rotor4D::identity(),
                    groundGray, groundGray
                });
                physWorld.addObject({x, -Tesseract::HS, z, w}, Tesseract::HS);
            }
        }
    }

    // Hypercubes floating above ground
    instances.push_back({
        {0.0f, Tesseract::HS, 0.0f, 0.0f},
        Math4D::Rotor4D::identity(),
        glm::vec3(0.2f, 0.4f, 1.0f),
        glm::vec3(1.0f, 0.8f, 0.2f)
    });
    physWorld.addObject({0.0f, Tesseract::HS, 0.0f, 0.0f}, Tesseract::HS);

    instances.push_back({
        {1.8f, Tesseract::HS, 1.2f, 0.8f},
        Math4D::Rotor4D::fromXZ(0.52f) * Math4D::Rotor4D::fromZW(0.31f),
        glm::vec3(1.0f, 0.2f, 0.2f),
        glm::vec3(1.0f, 1.0f, 0.2f)
    });
    physWorld.addObject({1.8f, Tesseract::HS, 1.2f, 0.8f}, Tesseract::HS);

    instances.push_back({
        {-1.8f, Tesseract::HS, 1.5f, -1.0f},
        Math4D::Rotor4D::fromXZ(1.1f),
        glm::vec3(0.2f, 1.0f, 0.2f),
        glm::vec3(0.8f, 0.2f, 1.0f)
    });
    physWorld.addObject({-1.8f, Tesseract::HS, 1.5f, -1.0f}, Tesseract::HS);

    instances.push_back({
        {1.5f, Tesseract::HS, -1.8f, 1.2f},
        Math4D::Rotor4D::fromXW(0.78f),
        glm::vec3(1.0f, 0.5f, 0.2f),
        glm::vec3(0.2f, 1.0f, 1.0f)
    });
    physWorld.addObject({1.5f, Tesseract::HS, -1.8f, 1.2f}, Tesseract::HS);

    instances.push_back({
        {-1.5f, Tesseract::HS, -1.2f, 0.9f},
        Math4D::Rotor4D::fromXZ(0.31f) * Math4D::Rotor4D::fromXW(0.52f),
        glm::vec3(0.8f, 0.2f, 0.8f),
        glm::vec3(0.2f, 1.0f, 0.5f)
    });
    physWorld.addObject({-1.5f, Tesseract::HS, -1.2f, 0.9f}, Tesseract::HS);

    return instances;
}

bool projectInstance(
    const Instance4D& inst,
    const glm::vec4& camPos,
    const Math4D::Rotor4D& camOrientation,
    float focalLength,
    const Tesseract& tesseract,
    std::vector<float>& outVertData
) {
    // Precompute rotation matrices once (eliminates per-vertex trig calls)
    glm::mat4 objM = inst.orientation.toMatrix();
    glm::mat4 camM = camOrientation.toMatrix();

    // Check if instance is culled (all vertices behind camera in W)
    bool behindCamera = true;
    for (int i = 0; i < 16; i++) {
        glm::vec4 v = objM * tesseract.verts4D[i];
        v += inst.pos - camPos;
        v = camM * v;
        if (-v.w > 0) { behindCamera = false; break; }
    }
    if (behindCamera) return false;

    // Project all vertices
    outVertData.clear();
    outVertData.reserve(16 * 6);

    for (int i = 0; i < 16; i++) {
        glm::vec4 localV = tesseract.verts4D[i];
        glm::vec4 v = objM * localV;
        v += inst.pos - camPos;
        v = camM * v;

        glm::vec3 p = Math4D::project4Dto3D(v.x, v.y, v.z, v.w, focalLength);

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

    return true;
}
