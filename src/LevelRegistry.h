#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Level.h"

// One entry per planned curriculum level. `factory` constructs the level lazily
// (only when the player selects it, so we don't build ten scenes up front).
// `implemented` distinguishes the fully-built Corridor from the placeholders.
struct LevelEntry {
    std::string                          name;
    bool                                 implemented;
    std::function<std::unique_ptr<Level>()> factory;
};

// The campaign, in teaching order. Defined in LevelRegistry.cpp.
const std::vector<LevelEntry>& levelRegistry();

// Construct a game scene by name, running scripts/scenes/<sceneName>.lua. Used by
// the menu's "Start game" button and by engine.goto_scene() transitions.
std::unique_ptr<Level> makeScene(const std::string& sceneName);
