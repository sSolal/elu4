#include "Level2.h"
#include "Math4D.h"
#include <cmath>

Level2Scene buildLevel2(PhysicsWorld& physWorld, const Object4D& treeObj) {
    Level2Scene scene;

    glm::vec3 groundGray(0.5f, 0.5f, 0.5f);

    // Ground grid: 9×9 on XZ plane at y = -HS (same as Level 1)
    for (float x = -2.4f; x <= 2.4f; x += 0.6f) {
        for (float z = -2.4f; z <= 2.4f; z += 0.6f) {
            scene.ground.push_back({
                {x, -Tesseract::HS, z, 0.0f},  // w=0 for flat ground
                0.0f, 0.0f, 0.0f,
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
        0.0f, 0.0f, 0.0f,
        green, brown
    });

    scene.trees.push_back({
        {0.0f, treeY, 0.0f, 0.0f},
        0.3f, 0.0f, 0.0f,
        green, brown
    });

    scene.trees.push_back({
        {1.5f, treeY, -1.5f, 0.0f},
        0.0f, 0.2f, 0.0f,
        green, brown
    });

    scene.trees.push_back({
        {-1.5f, treeY, 1.5f, 0.0f},
        0.1f, 0.0f, 0.1f,
        green, brown
    });

    scene.trees.push_back({
        {1.5f, treeY, 1.5f, 0.0f},
        0.0f, 0.15f, 0.0f,
        green, brown
    });

    return scene;
}

bool projectObjectInstance(
    const Object4D& obj,
    const ObjectInstance& inst,
    const glm::vec4& camPos,
    const Camera4D::Angles& angles,
    float focalLength,
    std::vector<float>& outVertData
) {
    // Check if instance is culled (all vertices behind camera in W)
    bool behindCamera = true;
    float cxw = angles.cxw, sxw = angles.sxw;
    float czw = angles.czw, szw = angles.szw;
    float cyw = angles.cyw, syw = angles.syw;

    // Early cull check
    for (const auto& v : obj.vertices) {
        float vx = v.x, vy = v.y, vz = v.z, vw = v.w;

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
    outVertData.reserve(obj.vertices.size() * 6);

    for (const auto& v : obj.vertices) {
        float vx = v.x, vy = v.y, vz = v.z, vw = v.w;

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

        // Color based on W coordinate  (use nominal range [-0.5, 0.5])
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
    }

    return true;  // not culled
}
