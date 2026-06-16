#include "Level2.h"
#include "Math4D.h"

Level2Scene buildLevel2(PhysicsWorld& physWorld, const Object4D& treeObj) {
    Level2Scene scene;

    glm::vec3 groundGray(0.5f, 0.5f, 0.5f);

    // Ground grid: 9×9 on XZ plane at y = -HS (same as Level 1)
    for (float x = -2.4f; x <= 2.4f; x += 0.6f) {
        for (float z = -2.4f; z <= 2.4f; z += 0.6f) {
            scene.ground.push_back({
                {x, -Tesseract::HS, z, 0.0f},
                Math4D::Rotor4D::identity(),
                groundGray, groundGray
            });
            physWorld.addObject({x, -Tesseract::HS, z, 0.0f}, Tesseract::HS);
        }
    }

    // Trees placed at various positions
    // Tree object extends from y=-1 to y=+3, so position at y=0.7 to have bottom at ground level y=-0.3
    glm::vec3 green(0.2f, 0.8f, 0.2f);
    glm::vec3 brown(0.5f, 0.3f, 0.1f);
    float treeY = 0.7f;  // offset so tree bottom aligns with ground

    scene.trees.push_back({
        {-1.5f, treeY, -1.5f, 0.0f},
        Math4D::Rotor4D::identity(),
        green, brown
    });

    scene.trees.push_back({
        {0.0f, treeY, 0.0f, 0.0f},
        Math4D::Rotor4D::fromXZ(0.3f),
        green, brown
    });

    scene.trees.push_back({
        {1.5f, treeY, -1.5f, 0.0f},
        Math4D::Rotor4D::fromXW(0.2f),
        green, brown
    });

    scene.trees.push_back({
        {-1.5f, treeY, 1.5f, 0.0f},
        Math4D::Rotor4D::fromXZ(0.1f) * Math4D::Rotor4D::fromZW(0.1f),
        green, brown
    });

    scene.trees.push_back({
        {1.5f, treeY, 1.5f, 0.0f},
        Math4D::Rotor4D::fromXW(0.15f),
        green, brown
    });

    return scene;
}

bool projectObjectInstance(
    const Object4D& obj,
    const ObjectInstance& inst,
    const glm::vec4& camPos,
    const Math4D::Rotor4D& camOrientation,
    float focalLength,
    std::vector<float>& outVertData
) {
    // Precompute rotation matrices once
    glm::mat4 objM = inst.orientation.toMatrix();
    glm::mat4 camM = camOrientation.toMatrix();

    // Early cull check
    bool behindCamera = true;
    for (const auto& v : obj.vertices) {
        glm::vec4 rv = objM * v;
        rv += inst.pos - camPos;
        rv = camM * rv;
        if (-rv.w > 0) { behindCamera = false; break; }
    }
    if (behindCamera) return false;

    // Project all vertices
    outVertData.clear();
    outVertData.reserve(obj.vertices.size() * 7);

    for (const auto& v : obj.vertices) {
        glm::vec4 rv = objM * v;
        rv += inst.pos - camPos;
        rv = camM * rv;

        glm::vec3 p = Math4D::project4Dto3D(rv.x, rv.y, rv.z, rv.w, focalLength);

        float t = (v.w + 0.5f);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        glm::vec3 c = glm::mix(inst.colorA, inst.colorB, t);

        outVertData.push_back(p.x);
        outVertData.push_back(p.y);
        outVertData.push_back(p.z);
        outVertData.push_back(c.r);
        outVertData.push_back(c.g);
        outVertData.push_back(c.b);
        outVertData.push_back(-rv.w);  // camera-relative 4D depth (in front => positive)
    }

    return true;
}
