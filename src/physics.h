#pragma once
#include <glm/glm.hpp>
#include <vector>

struct PhysicsAABB {
    glm::vec4 center;
    glm::vec4 half;     // per-axis half-extents (uniform cubes set all four equal)
};

struct PhysicsBody {
    glm::vec4 pos;
    float velY    = 0.0f;
    bool onGround = false;
    float radius  = 0.25f;  // uniform radius in all 4 dimensions
};

class PhysicsWorld {
public:
    static constexpr float GRAVITY = -9.8f;

    // Uniform-cube collider (the common case): half-extent equal on all 4 axes.
    void addObject(glm::vec4 center, float halfSize);
    // Box collider with independent per-axis half-extents (thin slabs / walls).
    void addBox(glm::vec4 center, glm::vec4 half);
    // One flat ground collider replacing a grid of per-tile cubes: a very wide,
    // thin box whose TOP face sits at surfaceY, spanning +/-halfExtent in X/Z/W.
    // The player lands on top; its lateral walls are far enough out never to snag.
    void addFlatGround(float surfaceY, float halfExtent, float thickness = 1000.0f);
    void step(PhysicsBody& body, glm::vec4 desiredMove, float dt);

private:
    std::vector<PhysicsAABB> objects_;
    void resolveCollision(PhysicsBody& body, const PhysicsAABB& aabb);
};
