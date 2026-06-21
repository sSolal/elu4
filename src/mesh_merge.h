#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "Object4D.h"
#include "Scene.h"     // ObjectInstance
#include "Math4D.h"

// Static-geometry merge: bake N placed copies of one base mesh into a SINGLE
// Object4D in world space. This is the generalisation of the trick that keeps
// BigVistaLevel (Level 10) fast: instead of submitting a field of tiles as N
// separate instances - each one a per-frame CPU projection + glBufferSubData
// re-upload + triangle depth-sort + draw call (and, if it occludes, a full-
// screen per-fragment 4D occlusion pass) - we fold them into one mesh that costs
// exactly ONE of each per frame.
//
// Use it for STATIC, NON-OCCLUDING scenery (floors, terrain backdrops, decorative
// props): geometry the player looks ACROSS, not through, so it needs no 4D
// hidden-surface test. The merged mesh sets occludes=false and leaves the
// occlusion hull / tetrahedra / triFace empty (the renderer only reads those when
// occludes==true). Do NOT merge walls or puzzle solids that must occlude in 4D -
// a merged mesh is not one convex solid and would be skipped from the occluder set
// anyway; keep those as instances (and cull them, cf. MazeLevel's PVS).
//
// The merged mesh reproduces the per-instance look exactly by baking colour into
// Object4D::vertexColors using the SAME rule projectObjectInstance applies
// (see Level2.cpp): the base mesh's own vertexColors when present, otherwise the
// instance colorA..colorB gradient keyed by the local vertex w (clamped w+0.5).
inline Object4D mergeInstances(const Object4D& base,
                               const std::vector<ObjectInstance>& instances,
                               bool occludes = false) {
    Object4D merged;
    merged.name     = base.name + " (merged)";
    merged.occludes = occludes;

    const size_t vbase = base.vertices.size();
    const bool perVertexColor = base.vertexColors.size() == vbase;

    merged.vertices.reserve(vbase * instances.size());
    merged.vertexColors.reserve(vbase * instances.size());
    merged.edges.reserve(base.edges.size() * instances.size());
    merged.triangleIndices.reserve(base.triangleIndices.size() * instances.size());

    for (const auto& inst : instances) {
        const glm::mat4 M = inst.orientation.toMatrix();   // local -> rotated, once per instance
        const unsigned int ofs = (unsigned int)merged.vertices.size();

        for (size_t vi = 0; vi < vbase; ++vi) {
            const glm::vec4& v = base.vertices[vi];
            merged.vertices.push_back(M * v + inst.pos);    // world space

            glm::vec3 c;
            if (perVertexColor) {
                c = base.vertexColors[vi];
            } else {
                float t = glm::clamp(v.w + 0.5f, 0.0f, 1.0f);
                c = glm::mix(inst.colorA, inst.colorB, t);
            }
            merged.vertexColors.push_back(c);
        }

        for (const auto& e : base.edges)
            merged.edges.push_back({e.first + (int)ofs, e.second + (int)ofs});

        for (unsigned int idx : base.triangleIndices)
            merged.triangleIndices.push_back(idx + ofs);
    }

    // triFace / tetrahedra / hullN / hullD intentionally left empty: only read on
    // the occludes==true path, which a merged backdrop never takes.
    return merged;
}

// The single identity instance a merged mesh is drawn with: it is already in world
// space, so it needs no further placement. Colours are unused (the mesh carries
// baked vertexColors), hence white.
inline std::vector<ObjectInstance> mergedInstance() {
    return { { glm::vec4(0.0f), Math4D::Rotor4D::identity(),
               glm::vec3(1.0f), glm::vec3(1.0f) } };
}
