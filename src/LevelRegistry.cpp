#include "LevelRegistry.h"

#include "levels/CorridorLevel.h"
#include "levels/DodgeballLevel.h"
#include "levels/TurnAndFaceLevel.h"
#include "levels/Level4ThirdPerson.h"
#include "levels/LeapLevel.h"
#include "levels/PlaneLevel.h"
#include "levels/PlaneLevel4D.h"
#include "levels/ForestFetchLevel.h"
#include "levels/MazeLevel.h"
#include "levels/BigVistaLevel.h"
#include "levels/ScriptedLevel.h"

const std::vector<LevelEntry>& levelRegistry() {
    // Built once on first call. Index 0 is the fully-playable Corridor; the rest
    // are placeholders whose design lives in docs/levels/ and which will be built
    // out one at a time.
    static const std::vector<LevelEntry> registry = [] {
        std::vector<LevelEntry> r;

        r.push_back({"1 - Corridor", true,
            [] { return std::unique_ptr<Level>(new CorridorLevel()); }});

        r.push_back({"2 - Dodgeball", true,
            [] { return std::unique_ptr<Level>(new DodgeballLevel()); }});

        r.push_back({"3 - Turn & Face", true,
            [] { return std::unique_ptr<Level>(new TurnAndFaceLevel()); }});

        r.push_back({"4 - Third Person", true,
            [] { return std::unique_ptr<Level>(new Level4ThirdPerson()); }});

        r.push_back({"5 - Leap", true,
            [] { return std::unique_ptr<Level>(new LeapLevel()); }});


        r.push_back({"6 - Plane (3D-like)", true,
            [] { return std::unique_ptr<Level>(new PlaneLevel()); }});

        r.push_back({"7 - Plane (4D course)", true,
            [] { return std::unique_ptr<Level>(new PlaneLevel4D()); }});

        r.push_back({"8 - Forest Fetch", true,
            [] { return std::unique_ptr<Level>(new ForestFetchLevel()); }});

        r.push_back({"9 - Maze", true,
            [] { return std::unique_ptr<Level>(new MazeLevel()); }});

        r.push_back({"10 - Big Vista", true,
            [] { return std::unique_ptr<Level>(new BigVistaLevel()); }});

        // --- Lua-scripted levels (run alongside the C++ ones during migration) ---
        r.push_back({"H - Hello (Lua)", true,
            [] { return std::unique_ptr<Level>(
                new ScriptedLevel("scripts/levels/hello.lua", "H - Hello (Lua)")); }});

        r.push_back({"6L - Plane (Lua)", true,
            [] { return std::unique_ptr<Level>(
                new ScriptedLevel("scripts/levels/plane.lua", "6L - Plane (Lua)")); }});

        r.push_back({"9L - Maze (Lua)", true,
            [] { return std::unique_ptr<Level>(
                new ScriptedLevel("scripts/levels/maze.lua", "9L - Maze (Lua)")); }});

        return r;
    }();

    return registry;
}
