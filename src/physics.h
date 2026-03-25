#pragma once
#include <glm/glm.hpp>
#include <vector>

struct PhysicsAABB {
    glm::vec4 center;
    float halfSize;
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

    void addObject(glm::vec4 center, float halfSize);
    void step(PhysicsBody& body, glm::vec4 desiredMove, float dt);

private:
    std::vector<PhysicsAABB> objects_;
    void resolveCollision(PhysicsBody& body, const PhysicsAABB& aabb);
};
