#include "Level2.h"
#include "Math4D.h"

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

    const bool perVertexColor = obj.vertexColors.size() == obj.vertices.size();
    for (size_t vi = 0; vi < obj.vertices.size(); ++vi) {
        const glm::vec4& v = obj.vertices[vi];
        glm::vec4 rv = objM * v;
        rv += inst.pos - camPos;
        rv = camM * rv;

        glm::vec3 p = Math4D::project4Dto3D(rv.x, rv.y, rv.z, rv.w, focalLength);

        glm::vec3 c;
        if (perVertexColor) {
            c = obj.vertexColors[vi];   // baked colour field (e.g. terrain by height)
        } else {
            float t = (v.w + 0.5f);
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            c = glm::mix(inst.colorA, inst.colorB, t);
        }

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
