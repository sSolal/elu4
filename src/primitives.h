#pragma once

#include "Object4D.h"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

// Portable population count (number of set bits). GCC/Clang expose
// __builtin_popcount; MSVC has __popcnt via <intrin.h>. The project is C++17,
// so std::popcount (C++20) isn't available.
#if defined(_MSC_VER)
  #include <intrin.h>
  static inline int popcount4d(unsigned x) { return (int)__popcnt(x); }
#else
  static inline int popcount4d(unsigned x) { return __builtin_popcount(x); }
#endif

// The engine's standard hypercube half-extent, used for shared marker/goal cubes
// (formerly Tesseract::HS). Keeps those markers their historical size.
constexpr float kHyperHalf = 0.3f;

// A 4D box (hyperrectangle) with independent half-extents per axis. Same
// topology as the unit hypercube (16 verts, 24 square faces) — only the vertex
// positions differ. Making a box THIN in one axis gives a flat slab that reads
// as a wall/floor rather than a chunky hypercube, which is what corridor walls
// want: the 4D analog of using a thin plane for a wall in 3D.
// Decompose a 3-cube into 6 tetrahedra (Kuhn subdivision around the main diagonal
// 0–7). Corner codes are 3-bit (bit a = the a-th free axis); shared by all callers.
static const int kBoxKuhn[6][4] = {
    {0, 1, 3, 7}, {0, 3, 2, 7}, {0, 2, 6, 7},
    {0, 6, 4, 7}, {0, 4, 5, 7}, {0, 5, 1, 7},
};

inline Object4D generateBox(const glm::vec4& half) {
    Object4D box;
    box.name = "Box";
    box.occludes = true;

    // 16 vertices: bit k of index i selects +half[k] (1) or -half[k] (0).
    for (int i = 0; i < 16; i++) {
        float x = ((i & 1) ? half.x : -half.x);
        float y = ((i & 2) ? half.y : -half.y);
        float z = ((i & 4) ? half.z : -half.z);
        float w = ((i & 8) ? half.w : -half.w);
        box.vertices.push_back(glm::vec4(x, y, z, w));
    }

    // Edges: vertices differing in exactly one coordinate.
    for (int i = 0; i < 16; i++) {
        for (int j = i + 1; j < 16; j++) {
            int diff = i ^ j;
            if ((diff & (diff - 1)) == 0) box.edges.push_back({i, j});
        }
    }

    // 8 cubic hyperfaces (facets), one per (axis k, sign s): outward normal s·e_k,
    // offset half[k]; the solid is { x : dot(n,x) <= half[k] }. facet index = k*2 + s.
    box.hullN.resize(8);
    box.hullD.resize(8);
    for (int k = 0; k < 4; k++) {
        for (int s = 0; s < 2; s++) {
            glm::vec4 nrm(0.0f);
            nrm[k] = (s ? 1.0f : -1.0f);
            box.hullN[k * 2 + s] = nrm;
            box.hullD[k * 2 + s] = half[k];
        }
    }

    // Tetrahedralise each cubic hyperface into 6 tets; all share the facet's normal.
    for (int k = 0; k < 4; k++) {
        for (int s = 0; s < 2; s++) {
            int facet = k * 2 + s;
            int fa[3], nf = 0;                       // the three free axes (≠ k), ascending
            for (int a = 0; a < 4; a++) if (a != k) fa[nf++] = a;
            auto vid = [&](int code) {
                int idx = s << k;                    // fix axis k at sign s
                if (code & 1) idx |= 1 << fa[0];
                if (code & 2) idx |= 1 << fa[1];
                if (code & 4) idx |= 1 << fa[2];
                return idx;
            };
            for (int t = 0; t < 6; t++) {
                box.tetrahedra.push_back(glm::ivec4(
                    vid(kBoxKuhn[t][0]), vid(kBoxKuhn[t][1]),
                    vid(kBoxKuhn[t][2]), vid(kBoxKuhn[t][3])));
                box.tetNormal.push_back(box.hullN[facet]);
                box.tetFace.push_back(facet);
            }
        }
    }

    // Cells (square faces): vertices differing in exactly two coordinates.
    for (int i = 0; i < 16; i++) {
        for (int j = i + 1; j < 16; j++) {
            if (popcount4d(i ^ j) != 2) continue;
            for (int k = 0; k < 16; k++) {
                if (k == i || k == j) continue;
                if (popcount4d(i ^ k) == 1 && popcount4d(j ^ k) == 1) {
                    int l = i ^ j ^ k;
                    if (i < j && i < k && i < l)
                        box.cells.push_back({i, k, j, l});  // cyclic order
                    break;
                }
            }
        }
    }

    // Rasterised surface = the square 2-faces, triangulated. Each square is the
    // intersection of its two fixed-axis facets and is drawn when either faces the eye.
    for (const auto& quad : box.cells) {
        if (quad.size() != 4) continue;
        glm::ivec2 facets(-1, -1);
        int nf = 0;
        for (int a = 0; a < 4; a++) {
            int bit = (quad[0] >> a) & 1;
            bool fixed = true;
            for (int q = 1; q < 4; q++)
                if (((quad[q] >> a) & 1) != bit) { fixed = false; break; }
            if (fixed && nf < 2) facets[nf++] = a * 2 + bit;
        }
        box.triangleIndices.push_back(quad[0]);
        box.triangleIndices.push_back(quad[1]);
        box.triangleIndices.push_back(quad[2]);
        box.triFace.push_back(facets);
        box.triangleIndices.push_back(quad[0]);
        box.triangleIndices.push_back(quad[2]);
        box.triangleIndices.push_back(quad[3]);
        box.triFace.push_back(facets);
    }

    return box;
}

// A ground / backdrop slab: identical geometry to generateBox but occludes=false.
//
// Occlusion convention (this is the single biggest per-level performance lever):
//   * Floors, terrain, distant scenery -> generateGround (or mergeInstances, which
//     also sets occludes=false). Grounds are looked ACROSS, not through, so they
//     need NO 4D hidden-surface test. That per-fragment occlusion pass (see
//     inner.frag occluded4D) is the dominant GPU cost and is pointless for a floor.
//   * Walls and any solid the player must not see THROUGH in 4D -> generateBox
//     (occludes=true), and keep them as instances culled to the 64-occluder budget
//     (cf. MazeLevel's PVS) rather than merged.
//   * Tesseract / hypersphere puzzle objects -> keep occludes=true (self-occlusion
//     is the whole 4D effect).
inline Object4D generateGround(const glm::vec4& half) {
    Object4D g = generateBox(half);
    g.name     = "Ground";
    g.occludes = false;
    return g;
}

// A polyline of `pointCount` vertices joined by pointCount-1 consecutive edges.
// Vertices start at the origin — the caller overwrites mesh.vertices with the
// actual 4D points each frame (the count is fixed so the GPU buffer is allocated
// once). No cells/triangleIndices: this is a pure line, drawn as GL_LINES.
inline Object4D generatePolyline(int pointCount) {
    Object4D line;
    line.name = "Polyline";
    if (pointCount < 2) pointCount = 2;
    line.vertices.assign((size_t)pointCount, glm::vec4(0.0f));
    for (int i = 0; i + 1 < pointCount; ++i)
        line.edges.push_back({i, i + 1});
    return line;
}

// The unit hypercube is just a box with half-extent 0.5 on every axis.
inline Object4D generateHypercube() {
    Object4D hypercube = generateBox(glm::vec4(0.5f));
    hypercube.name = "Hypercube";
    return hypercube;
}

// A low-poly hypersphere as the 4D cross-polytope (16-cell): 8 vertices (±radius on
// each axis) and 16 cells that are EACH a single regular tetrahedron — the cleanest
// genuine-tetrahedral 4D ball (the 4D analog of approximating a 3D sphere with an
// octahedron). It is convex, so it occludes like every other solid; its 16 facet
// hyperplanes are the L1 constraints sum(±x) <= radius. Recognisable and cheap; bump
// to a 24-/600-cell later if a smoother ball is wanted.
inline Object4D generateHypersphere(float radius = 0.5f) {
    Object4D s;
    s.name = "Hypersphere";
    s.occludes = true;

    // 8 vertices: vid(axis i, sign bit b) = i*2 + b, at (b? +radius : -radius)·e_i.
    auto vid = [](int axis, int bit) { return axis * 2 + bit; };
    s.vertices.resize(8);
    for (int i = 0; i < 4; ++i)
        for (int b = 0; b < 2; ++b) {
            glm::vec4 v(0.0f);
            v[i] = (b ? radius : -radius);
            s.vertices[vid(i, b)] = v;
        }

    // Edges: every vertex pair except antipodal (same axis, opposite sign).
    for (int p = 0; p < 8; ++p)
        for (int q = p + 1; q < 8; ++q)
            if (p / 2 != q / 2) s.edges.push_back({p, q});

    // 16 tetrahedral cells (facets), one per sign code c (bit i = sign of axis i). The
    // cell picks one vertex per axis; its outward normal is the unit sign vector and
    // the facet plane is dot(n, x) = radius/2 (so the solid is the L1 ball).
    s.hullN.resize(16);
    s.hullD.resize(16);
    for (int c = 0; c < 16; ++c) {
        glm::vec4 n(0.0f);
        glm::ivec4 tet;
        for (int i = 0; i < 4; ++i) {
            int b = (c >> i) & 1;
            n[i] = (b ? 0.5f : -0.5f);     // |n| = sqrt(4·0.25) = 1
            tet[i] = vid(i, b);
        }
        s.hullN[c] = n;
        s.hullD[c] = radius * 0.5f;
        s.tetrahedra.push_back(tet);
        s.tetNormal.push_back(n);
        s.tetFace.push_back(c);
    }

    // Rasterised surface = the 32 triangular 2-faces (the 2-skeleton). A 2-face omits
    // one axis m and fixes a sign on the other three; it is shared by the two cells
    // that extend it with ±e_m, so it is drawn when either of those faces the eye.
    for (int m = 0; m < 4; ++m) {
        int oa[3], n = 0;
        for (int a = 0; a < 4; ++a) if (a != m) oa[n++] = a;
        for (int s0 = 0; s0 < 2; ++s0)
        for (int s1 = 0; s1 < 2; ++s1)
        for (int s2 = 0; s2 < 2; ++s2) {
            int base = (s0 << oa[0]) | (s1 << oa[1]) | (s2 << oa[2]);
            glm::ivec2 facets(base, base | (1 << m));   // axis-m sign 0 and 1
            s.triangleIndices.push_back(vid(oa[0], s0));
            s.triangleIndices.push_back(vid(oa[1], s1));
            s.triangleIndices.push_back(vid(oa[2], s2));
            s.triFace.push_back(facets);
        }
    }

    return s;
}
