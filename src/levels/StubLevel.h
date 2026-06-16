#pragma once

#include <string>
#include <vector>
#include "Level.h"
#include "Scene.h"

// A single reusable placeholder used for every curriculum level that isn't built
// yet (Dodgeball through Big Vista). It loads a minimal marker scene, allows
// free look/movement, and shows a "not yet built" HUD note. The registry
// constructs one per planned level with its name and a one-line teaching blurb.
class StubLevel : public Level {
public:
    StubLevel(std::string name, std::string blurb)
        : name_(std::move(name)), blurb_(std::move(blurb)) {}

    const char* name() const override { return name_.c_str(); }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    void renderHUD(const LevelContext& ctx) override;

private:
    std::string name_;
    std::string blurb_;
    std::vector<Instance4D> scene_;
};
