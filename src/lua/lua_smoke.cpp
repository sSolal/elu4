// Headless smoke test for the core Lua bindings (no GL, no game state).
// Constructs vec3/vec4/Rotor4D from Lua, exercises operators, and — crucially —
// checks that writing a field through a userdata reference mutates the real
// object (the value-semantics trap flagged in the plan). Run via `ctest`.
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <sol/sol.hpp>

#include "lua/LuaBindings.h"

static int g_failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++g_failures;
    }
}

int main() {
    sol::state L;
    L.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);
    lua::bindCoreTypes(L);

    auto run = [&](const char* src, const char* label) {
        sol::protected_function_result r = L.safe_script(src, sol::script_pass_on_error);
        if (!r.valid()) {
            sol::error err = r;
            std::fprintf(stderr, "  FAIL (%s): %s\n", label, err.what());
            ++g_failures;
        }
    };

    // vec4: construction, arithmetic, length.
    run(R"(
        local a = vec4(1, 2, 3, 4)
        assert(a.x == 1 and a.w == 4, "vec4 fields")
        local b = a + vec4(1, 1, 1, 1)
        assert(b.x == 2 and b.w == 5, "vec4 add")
        local c = a * 2.0
        assert(c.z == 6, "vec4 scalar mul")
        assert(math.abs((vec4(3,4,0,0)):length() - 5.0) < 1e-5, "vec4 length")
    )", "vec4");

    // The mutation trap: writing a field on a userdata must persist.
    run(R"(
        local p = vec4(0, 0, 0, 0)
        p.x = 7.5
        p.w = -2.0
        assert(p.x == 7.5 and p.w == -2.0, "vec4 in-place field write")
    )", "vec4-mutation");

    // vec3 colors.
    run(R"(
        local col = vec3(1, 0.5, 0.25)
        assert(col.y == 0.5, "vec3 fields")
    )", "vec3");

    // Rotor4D: identity, factories, composition, forward vector.
    run(R"(
        local id = Rotor4D.identity()
        local r  = Rotor4D.from_xw(math.rad(90))
        local c  = r * id            -- rotor * rotor
        assert(c ~= nil, "rotor compose")
        local nose = id:forward()    -- (0,0,0,-1) under identity
        assert(math.abs(nose.w + 1.0) < 1e-4 and math.abs(nose.x) < 1e-4, "identity forward")
        -- rotor applied to a vec4
        local v = id * vec4(1, 2, 3, 4)
        assert(math.abs(v.x - 1) < 1e-4 and math.abs(v.w - 4) < 1e-4, "identity rotate")
    )", "rotor");

    // normalize() mutates in place and yields a unit rotor.
    run(R"(
        local r = Rotor4D.from_yw(math.rad(30)) + Rotor4D.from_zw(math.rad(20))
        r:normalize()
        local n = Rotor4D.dot(r, r)
        assert(math.abs(n - 1.0) < 1e-4, "normalize -> unit, got "..tostring(n))
    )", "rotor-normalize");

    if (g_failures == 0) {
        std::printf("lua_smoke: all core-binding checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "lua_smoke: %d failure(s)\n", g_failures);
    return 1;
}
