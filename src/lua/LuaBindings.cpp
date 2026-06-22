#include "lua/LuaBindings.h"

#include <glm/glm.hpp>
#include <string>

#include "Math4D.h"

namespace lua {

namespace {

std::string vec4ToString(const glm::vec4& v) {
    return "vec4(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " +
           std::to_string(v.z) + ", " + std::to_string(v.w) + ")";
}

std::string vec3ToString(const glm::vec3& v) {
    return "vec3(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " +
           std::to_string(v.z) + ")";
}

}  // namespace

void bindCoreTypes(sol::state& L) {
    // --- glm::vec3 (colors, minimap map-space points) ----------------------
    L.new_usertype<glm::vec3>(
        "vec3",
        sol::call_constructor,
        sol::constructors<glm::vec3(), glm::vec3(float),
                          glm::vec3(float, float, float)>(),
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z,
        sol::meta_function::addition,
        [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
        sol::meta_function::subtraction,
        [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
        sol::meta_function::unary_minus, [](const glm::vec3& a) { return -a; },
        sol::meta_function::multiplication,
        sol::overload([](const glm::vec3& a, float k) { return a * k; },
                      [](float k, const glm::vec3& a) { return a * k; },
                      [](const glm::vec3& a, const glm::vec3& b) { return a * b; }),
        "length", [](const glm::vec3& a) { return glm::length(a); },
        "dot", [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); },
        "normalize", [](const glm::vec3& a) { return glm::normalize(a); },
        sol::meta_function::to_string, &vec3ToString);

    // --- glm::vec4 (4D positions / half-extents) ---------------------------
    L.new_usertype<glm::vec4>(
        "vec4",
        sol::call_constructor,
        sol::constructors<glm::vec4(), glm::vec4(float),
                          glm::vec4(float, float, float, float)>(),
        "x", &glm::vec4::x,
        "y", &glm::vec4::y,
        "z", &glm::vec4::z,
        "w", &glm::vec4::w,
        sol::meta_function::addition,
        [](const glm::vec4& a, const glm::vec4& b) { return a + b; },
        sol::meta_function::subtraction,
        [](const glm::vec4& a, const glm::vec4& b) { return a - b; },
        sol::meta_function::unary_minus, [](const glm::vec4& a) { return -a; },
        sol::meta_function::multiplication,
        sol::overload([](const glm::vec4& a, float k) { return a * k; },
                      [](float k, const glm::vec4& a) { return a * k; },
                      [](const glm::vec4& a, const glm::vec4& b) { return a * b; }),
        "length", [](const glm::vec4& a) { return glm::length(a); },
        "dot", [](const glm::vec4& a, const glm::vec4& b) { return glm::dot(a, b); },
        sol::meta_function::to_string, &vec4ToString);

    // --- Math4D::Rotor4D (4D orientation) ----------------------------------
    // Static factories are exposed as Rotor4D.from_xw(angle) etc.; methods
    // (reverse/normalize/forward) operate on a rotor value. normalize() mutates
    // the userdata in place. forward() is the nose vector every level computes:
    // reverse().toMatrix() * (0,0,0,-1).
    using R = Math4D::Rotor4D;
    L.new_usertype<R>(
        "Rotor4D",
        sol::no_constructor,
        "identity", &R::identity,
        "from_xy", &R::fromXY,
        "from_xz", &R::fromXZ,
        "from_xw", &R::fromXW,
        "from_yz", &R::fromYZ,
        "from_yw", &R::fromYW,
        "from_zw", &R::fromZW,
        "dot", &R::dot,
        "nlerp", &R::nlerp,
        "reverse", &R::reverse,
        "normalize", &R::normalize,        // in-place mutation
        "rotate", [](const R& r, const glm::vec4& v) { return r.rotate(v); },
        "forward",
        [](const R& r) {
            return glm::vec4(r.reverse().toMatrix() * glm::vec4(0, 0, 0, -1));
        },
        sol::meta_function::addition,
        [](const R& a, const R& b) { return a + b; },
        sol::meta_function::multiplication,
        sol::overload([](const R& a, const R& b) { return a * b; },
                      [](const R& a, float k) { return a * k; },
                      [](const R& a, const glm::vec4& v) {
                          return glm::vec4(a.toMatrix() * v);
                      }));
}

}  // namespace lua
