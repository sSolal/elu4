#include "LevelRegistry.h"

#include "levels/CorridorLevel.h"
#include "levels/DodgeballLevel.h"
#include "levels/TurnAndFaceLevel.h"
#include "levels/StubLevel.h"

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

        auto stub = [&r](const char* name, const char* blurb) {
            std::string n = name, b = blurb;
            r.push_back({n, false,
                [n, b] { return std::unique_ptr<Level>(new StubLevel(n, b)); }});
        };

        r.push_back({"3 - Turn & Face", true,
            [] { return std::unique_ptr<Level>(new TurnAndFaceLevel()); }});

        stub("4 - Third Person",     "Steer a 4D avatar with an orbiting camera; run, strafe, turn, jump.");
        stub("5 - Leap",             "Platforming: time your jumps from platform to platform.");
        stub("6 - Plane (3D-like)",  "Fly through hoops; level flight is just like a 3D plane.");
        stub("7 - Plane (4D course)","Now the hoops curve through 4D - use every direction.");
        stub("8 - Forest Fetch",     "Find the golden object, return it to the NPC, follow directions onward.");
        stub("9 - Maze",             "Navigate a 4D maze from entrance to exit.");
        stub("10 - Big Vista",       "Read distance: near and far detail in one large scene.");

        return r;
    }();

    return registry;
}
