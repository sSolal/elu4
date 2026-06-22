#include "physics.h"
#include <cmath>

void PhysicsWorld::addObject(glm::vec4 center, float halfSize) {
    objects_.push_back({center, glm::vec4(halfSize)});
}

void PhysicsWorld::addBox(glm::vec4 center, glm::vec4 half) {
    objects_.push_back({center, half});
}

void PhysicsWorld::addFlatGround(float surfaceY, float halfExtent, float thickness) {
    // Top face at surfaceY => center.y = surfaceY - thickness; huge in X/Z/W.
    glm::vec4 center(0.0f, surfaceY - thickness, 0.0f, 0.0f);
    glm::vec4 half(halfExtent, thickness, halfExtent, halfExtent);
    objects_.push_back({center, half});
}

void PhysicsWorld::step(PhysicsBody& body, glm::vec4 desiredMove, float dt) {
    // Apply gravity (scaled per-body: gravityScale = 0 disables it, so a level can
    // float an object — e.g. the plane — and drive its motion entirely by script).
    body.velY += GRAVITY * body.gravityScale * dt;

    // Combine gravity with desired move
    desiredMove.y = body.velY * dt;

    // Apply movement
    body.pos += desiredMove;

    // Resolve collisions (3 passes for stability in tight corners)
    body.onGround = false;
    for (int pass = 0; pass < 3; ++pass) {
        for (const auto& obj : objects_) {
            resolveCollision(body, obj);
        }
    }
}

void PhysicsWorld::resolveCollision(PhysicsBody& body, const PhysicsAABB& aabb) {
    float pr = body.radius;

    // Compute overlap along each axis (per-axis half-extent + the body radius).
    float dx = (pr + aabb.half.x) - std::abs(body.pos.x - aabb.center.x);
    float dy = (pr + aabb.half.y) - std::abs(body.pos.y - aabb.center.y);
    float dz = (pr + aabb.half.z) - std::abs(body.pos.z - aabb.center.z);
    float dw = (pr + aabb.half.w) - std::abs(body.pos.w - aabb.center.w);

    // No collision if any axis has no overlap
    if (dx <= 0.0f || dy <= 0.0f || dz <= 0.0f || dw <= 0.0f) {
        return;
    }

    // Find minimum overlap axis (MTV)
    float overlaps[4] = {dx, dy, dz, dw};
    int minAxis = 0;
    for (int i = 1; i < 4; ++i) {
        if (overlaps[i] < overlaps[minAxis]) {
            minAxis = i;
        }
    }

    // Determine push direction and push out
    float depth = overlaps[minAxis];
    float bodyCoord, aabbCoord;

    if (minAxis == 0) {
        bodyCoord = body.pos.x;
        aabbCoord = aabb.center.x;
    } else if (minAxis == 1) {
        bodyCoord = body.pos.y;
        aabbCoord = aabb.center.y;
    } else if (minAxis == 2) {
        bodyCoord = body.pos.z;
        aabbCoord = aabb.center.z;
    } else {
        bodyCoord = body.pos.w;
        aabbCoord = aabb.center.w;
    }

    float sign = bodyCoord >= aabbCoord ? 1.0f : -1.0f;

    // Apply push
    if (minAxis == 0) {
        body.pos.x += depth * sign;
    } else if (minAxis == 1) {
        body.pos.y += depth * sign;
    } else if (minAxis == 2) {
        body.pos.z += depth * sign;
    } else {
        body.pos.w += depth * sign;
    }

    // Handle vertical velocity for Y-axis collisions
    if (minAxis == 1) {
        if (sign > 0.0f && body.velY < 0.0f) {
            // Pushing up = landing on top
            body.velY = 0.0f;
            body.onGround = true;
        } else if (sign < 0.0f && body.velY > 0.0f) {
            // Pushing down = hit ceiling
            body.velY = 0.0f;
        }
    }
}
