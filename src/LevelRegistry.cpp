#include "LevelRegistry.h"

#include "levels/ScriptedLevel.h"

const std::vector<LevelEntry>& levelRegistry() {
    // The campaign, in teaching order. Every level is a Lua script under
    // scripts/levels/ run through ScriptedLevel — the original C++ level classes
    // were removed once the port was complete (the engine now runs entirely on the
    // scripting layer). Built once on first call; each entry constructs its
    // ScriptedLevel lazily on selection.
    static const std::vector<LevelEntry> registry = [] {
        auto scripted = [](const char* path, const char* label) {
            return LevelEntry{label, true,
                [path, label] { return std::unique_ptr<Level>(new ScriptedLevel(path, label)); }};
        };

        std::vector<LevelEntry> r;
        r.push_back(scripted("scripts/levels/corridor.lua",    "1 - Corridor"));
        r.push_back(scripted("scripts/levels/dodgeball.lua",   "2 - Dodgeball"));
        r.push_back(scripted("scripts/levels/turnface.lua",    "3 - Turn & Face"));
        r.push_back(scripted("scripts/levels/thirdperson.lua", "4 - Third Person"));
        r.push_back(scripted("scripts/levels/leap.lua",        "5 - Leap"));
        r.push_back(scripted("scripts/levels/plane.lua",       "6 - Plane (3D-like)"));
        r.push_back(scripted("scripts/levels/plane4d.lua",     "7 - Plane (4D course)"));
        r.push_back(scripted("scripts/levels/forest.lua",      "8 - Forest Fetch"));
        r.push_back(scripted("scripts/levels/maze.lua",        "9 - Maze"));
        r.push_back(scripted("scripts/levels/bigvista.lua",    "10 - Big Vista"));
        return r;
    }();

    return registry;
}
