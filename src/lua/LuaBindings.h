#pragma once

// All sol2 usage is confined to the .cpp translation units that include this
// header (LuaBindings.cpp, ScriptedLevel.cpp). Keep this out of game-wide
// headers so sol2's heavy template machinery never leaks into the rest of the
// build. ScriptedLevel.h forward-declares sol::state via <sol/forward.hpp>.
#include <sol/sol.hpp>

namespace lua {

// Bind the value types every level script needs: glm::vec3, glm::vec4, and
// Math4D::Rotor4D. These are pure value usertypes (no engine/GL state), so they
// are bound once into any sol::state — both the game host and the headless
// smoke test reuse this.
void bindCoreTypes(sol::state& L);

}  // namespace lua
