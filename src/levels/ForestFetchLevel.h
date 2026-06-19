#pragma once

#include <vector>
#include <string>
#include "Level.h"
#include "Object4D.h"
#include "ObjectBuffer.h"
#include "Renderer.h"   // ObjectInstance (via Level2.h)

// Level 8 - Forest Fetch.
//
// Teaches mapping the space: the forest *floor* is the 3-volume X-Z-W (Y is up),
// so you can wander in three ground directions and genuinely get lost - something
// impossible on a flat 3D floor. There is no new movement primitive; the player
// has full, unrestricted first-person controls. The challenge is orientation and
// memory, navigating by named landmark trees and axis-phrased directions.
//
// Quest (two legs): an NPC at the hub asks you to fetch a golden cube hidden deep
// toward +W past the Tall Red Pine. You carry it back, the NPC then directs you to
// a clearing toward -X past the Crooked Blue Tree, and reaching it wins.
class ForestFetchLevel : public Level {
public:
    ForestFetchLevel();
    ~ForestFetchLevel() override;

    const char* name() const override { return "8 - Forest Fetch"; }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    bool checkWin() const override { return won_; }
    void renderHUD(const LevelContext& ctx) override;

private:
    // Quest progression. Space advances Ask->Fetching (accept), Fetching->Returning
    // (pick up the cube), Returning->Directed (deliver it); Directed->Done is
    // automatic on reaching the clearing.
    enum class Q { Ask, Fetching, Returning, Directed, Done };

    struct Landmark { glm::vec4 pos; const char* label; glm::vec3 color; };

    // --- Meshes + GPU buffers ---
    Object4D     treeMesh_,     landmarkMesh_, groundMesh_, npcMesh_, goldMesh_;
    ObjectBuffer treeBuf_,      landmarkBuf_,  groundBuf_,  npcBuf_,  goldBuf_;

    // --- Static scene instances ---
    std::vector<ObjectInstance> groundInsts_;
    std::vector<ObjectInstance> treeInsts_;
    std::vector<ObjectInstance> landmarkInsts_;
    std::vector<Landmark>       landmarks_;

    // --- Quest state ---
    Q         quest_   = Q::Ask;
    bool      held_    = false;   // carrying the golden cube
    bool      won_     = false;
    bool      spaceWasDown_ = false;

    glm::vec4 npcPos_{0.0f};
    glm::vec4 goldPos_{0.0f};
    glm::vec4 finalPos_{0.0f};

    // Proximity flags, refreshed each frame for the HUD prompt.
    bool nearNpc_  = false;
    bool nearGold_ = false;
    bool atFinal_  = false;

    // --- Tunables ---
    static constexpr float SURFACE_Y    = 0.0f;   // top of the ground
    static constexpr float EYE_HEIGHT   = 1.6f;   // player eye / body radius
    static constexpr float TILE         = 6.0f;   // ground cell half-extent (X/Z/W)
    static constexpr float TILE_Y       = 0.25f;  // ground slab thinness in Y
    static constexpr int   NT           = 2;      // ground cells per axis = 2*NT+1 (5 -> 125 tiles)
    static constexpr float STEP         = 12.0f;  // ground cell spacing = 2*TILE (cells abut)
    static constexpr float TREE_SCALE   = 2.5f;   // ordinary trees
    static constexpr float LM_SCALE     = 4.5f;   // landmark trees (taller, distinct colour)
    static constexpr int   TREE_N       = 40;     // scattered ordinary trees
    static constexpr float CLEARING_R   = 7.0f;   // keep trees out of hub/gold/final
    static constexpr float INTERACT_DIST = 4.5f;  // SPACE range for NPC / gold
    static constexpr float ARRIVE_DIST   = 5.0f;  // auto-win range at the clearing
    static constexpr float HOLD_DIST     = 1.6f;  // how far in front the carried cube sits
};
