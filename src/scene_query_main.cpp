// 4dscene — a headless tool to reason about 4D scenes from the CLI.
//
// Place labelled 4D points, move/rotate a 4D camera, then ask intuitive
// questions ("is P on the left or the right?", "is it in front or behind?")
// or assert invariants that should hold (a distance is preserved under camera
// motion). No OpenGL: it reuses the exact same camera transform and flat-horizon
// control model as the game (see src/Camera.h, src/Scene.cpp), so answers match.
//
//   camera-space = camOrientation.toMatrix() * (worldPoint - camPos)
//     x > 0 -> RIGHT     x < 0 -> LEFT
//     y > 0 -> ABOVE     y < 0 -> BELOW
//     w < 0 -> IN FRONT  w > 0 -> BEHIND   (camera looks along -W)
//   screen (x,y) come from the same W-perspective projection as the renderer.
//
// Flat-horizon camera: "camera rotate yw" is look up/down (clamped pitch);
// "camera rotate xw|zw|xz" is horizontal heading (never tilts world-up Y).
// xy/yz are rejected because they would roll the horizon.
//
// Usage:
//   4dscene              # interactive REPL on stdin
//   4dscene file.scene   # run a scenario script, exit non-zero if any assert fails
//
// Commands (one per line; '#' starts a comment):
//   point <name> <x> <y> <z> <w>          place / move a labelled point
//   camera pos <x> <y> <z> <w>            set camera position
//   camera rotate yw <deg>                look up/down (clamped pitch)
//   camera rotate <xw|zw|xz> <deg>        horizontal heading (flat horizon)
//   camera focal <f>                      set focal length (default 2.0)
//   camera reset                          reset camera to origin / identity
//   query <name>                          print full classification of a point
//   where <name>                          one-line human summary
//   assert <name> <left|right|above|below|front|behind>
//   assert dist <a> <b> <expected> [eps]  4D distance between two points
//   list                                  dump all points + camera
//   echo <text...>                        print a message (handy in scripts)
//   help

#include "Math4D.h"
#include <glm/glm.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Mirrors the game's flat-horizon Camera4D exactly (see src/Camera.h):
// heading lives in `yaw` (planes within {X,Z,W}, never touching Y) and look
// up/down is a single clamped `pitch` applied OUTSIDE the yaw. So world-up Y
// only ever maps into the camera Y-W plane and the horizon never rolls.
struct Camera {
    glm::vec4 pos{0.0f};
    Math4D::Rotor4D yaw = Math4D::Rotor4D::identity();
    float pitch = 0.0f;  // degrees, clamped +/-89
    float focal = 2.0f;

    void reset() {
        pos = glm::vec4(0.0f);
        yaw = Math4D::Rotor4D::identity();
        pitch = 0.0f;
        focal = 2.0f;
    }

    Math4D::Rotor4D orientation() const {
        return Math4D::Rotor4D::fromYW(pitch * 3.14159265358979323846f / 180.0f) * yaw;
    }

    // Horizontal turn: compose into yaw (local frame), then re-normalize.
    void turn(const Math4D::Rotor4D& delta) {
        yaw = delta * yaw;
        yaw.normalize();
    }
};

struct Classification {
    glm::vec4 cam;     // point in camera space
    glm::vec3 screen;  // projected 3D position
    bool inFront;
};

Classification classify(const Camera& cam, const glm::vec4& world) {
    Classification c;
    c.cam = cam.orientation().toMatrix() * (world - cam.pos);
    c.screen = Math4D::project4Dto3D(c.cam.x, c.cam.y, c.cam.z, c.cam.w, cam.focal);
    c.inFront = (c.cam.w < 0.0f);  // camera looks along -W
    return c;
}

const char* lr(float x)   { return x > 0 ? "RIGHT" : (x < 0 ? "LEFT" : "CENTER"); }
const char* ud(float y)   { return y > 0 ? "ABOVE" : (y < 0 ? "BELOW" : "LEVEL"); }
const char* fb(bool f)    { return f ? "IN FRONT" : "BEHIND"; }

Math4D::Rotor4D rotorForPlane(const std::string& plane, float rad, bool& ok) {
    ok = true;
    if (plane == "xy") return Math4D::Rotor4D::fromXY(rad);
    if (plane == "xz") return Math4D::Rotor4D::fromXZ(rad);
    if (plane == "xw") return Math4D::Rotor4D::fromXW(rad);
    if (plane == "yz") return Math4D::Rotor4D::fromYZ(rad);
    if (plane == "yw") return Math4D::Rotor4D::fromYW(rad);
    if (plane == "zw") return Math4D::Rotor4D::fromZW(rad);
    ok = false;
    return Math4D::Rotor4D::identity();
}

constexpr float PI = 3.14159265358979323846f;
float deg2rad(float d) { return d * PI / 180.0f; }

// State shared across commands.
std::map<std::string, glm::vec4> g_points;
Camera g_camera;
int g_failures = 0;

bool getPoint(const std::string& name, glm::vec4& out) {
    auto it = g_points.find(name);
    if (it == g_points.end()) {
        std::cout << "  error: no point named '" << name << "'\n";
        return false;
    }
    out = it->second;
    return true;
}

void printUsage() {
    std::cout <<
        "commands:\n"
        "  point <name> <x> <y> <z> <w>\n"
        "  camera pos <x> <y> <z> <w>\n"
        "  camera rotate yw <deg>            (look up/down, clamped pitch)\n"
        "  camera rotate <xw|zw|xz> <deg>    (horizontal heading, flat horizon)\n"
        "  camera focal <f>\n"
        "  camera reset\n"
        "  query <name>          where <name>          list\n"
        "  assert <name> <left|right|above|below|front|behind>\n"
        "  assert at <name> <x> <y> <z> <w> [eps]   (exact camera-space position)\n"
        "  assert dist <a> <b> <expected> [eps]\n"
        "  echo <text>           help\n";
}

// Returns false only to signal "quit".
bool handle(const std::string& line) {
    std::istringstream in(line);
    std::string cmd;
    if (!(in >> cmd)) return true;            // blank line
    if (cmd.empty() || cmd[0] == '#') return true;
    if (cmd == "quit" || cmd == "exit") return false;

    if (cmd == "help") { printUsage(); return true; }

    if (cmd == "echo") {
        std::string rest;
        std::getline(in, rest);
        if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
        std::cout << rest << "\n";
        return true;
    }

    if (cmd == "point") {
        std::string name; glm::vec4 p;
        if (in >> name >> p.x >> p.y >> p.z >> p.w) {
            g_points[name] = p;
            std::cout << "  point " << name << " = (" << p.x << ", " << p.y
                      << ", " << p.z << ", " << p.w << ")\n";
        } else std::cout << "  usage: point <name> <x> <y> <z> <w>\n";
        return true;
    }

    if (cmd == "camera") {
        std::string sub;
        if (!(in >> sub)) { std::cout << "  usage: camera <pos|rotate|focal|reset>\n"; return true; }
        if (sub == "pos") {
            glm::vec4 p;
            if (in >> p.x >> p.y >> p.z >> p.w) {
                g_camera.pos = p;
                std::cout << "  camera at (" << p.x << ", " << p.y << ", " << p.z << ", " << p.w << ")\n";
            } else std::cout << "  usage: camera pos <x> <y> <z> <w>\n";
        } else if (sub == "rotate") {
            std::string plane; float deg;
            if (in >> plane >> deg) {
                if (plane == "yw") {
                    // Look up/down: clamped pitch, applied outside the yaw.
                    g_camera.pitch = glm::clamp(g_camera.pitch + deg, -89.0f, 89.0f);
                    std::cout << "  camera pitch = " << g_camera.pitch << " deg\n";
                } else if (plane == "xw" || plane == "zw" || plane == "xz") {
                    // Horizontal heading: stays within {X,Z,W}, never tilts Y.
                    bool ok;
                    g_camera.turn(rotorForPlane(plane, deg2rad(deg), ok));
                    std::cout << "  camera turned " << deg << " deg in " << plane << "\n";
                } else if (plane == "xy" || plane == "yz") {
                    std::cout << "  '" << plane << "' would roll the horizon; the flat-horizon "
                                 "camera only supports yaw (xw/zw/xz) and pitch (yw)\n";
                } else {
                    std::cout << "  unknown plane '" << plane << "'\n";
                }
            } else std::cout << "  usage: camera rotate <plane> <deg>\n";
        } else if (sub == "focal") {
            float f;
            if (in >> f) { g_camera.focal = f; std::cout << "  focal = " << f << "\n"; }
            else std::cout << "  usage: camera focal <f>\n";
        } else if (sub == "reset") {
            g_camera.reset();
            std::cout << "  camera reset\n";
        } else std::cout << "  unknown camera subcommand '" << sub << "'\n";
        return true;
    }

    if (cmd == "query") {
        std::string name; glm::vec4 p;
        if (!(in >> name) || !getPoint(name, p)) return true;
        Classification c = classify(g_camera, p);
        std::cout << "  " << name << ": cam-space (" << c.cam.x << ", " << c.cam.y
                  << ", " << c.cam.z << ", " << c.cam.w << ")\n"
                  << "    screen (" << c.screen.x << ", " << c.screen.y << ")  "
                  << lr(c.cam.x) << " / " << ud(c.cam.y) << " / " << fb(c.inFront) << "\n";
        return true;
    }

    if (cmd == "where") {
        std::string name; glm::vec4 p;
        if (!(in >> name) || !getPoint(name, p)) return true;
        Classification c = classify(g_camera, p);
        std::cout << "  " << name << " is to the " << lr(c.cam.x) << ", "
                  << ud(c.cam.y) << ", " << fb(c.inFront) << "\n";
        return true;
    }

    if (cmd == "list") {
        std::cout << "  camera: pos (" << g_camera.pos.x << ", " << g_camera.pos.y << ", "
                  << g_camera.pos.z << ", " << g_camera.pos.w << ")  pitch " << g_camera.pitch
                  << " deg  focal " << g_camera.focal << "\n";
        for (const auto& kv : g_points) {
            const glm::vec4& p = kv.second;
            std::cout << "  point " << kv.first << " = (" << p.x << ", " << p.y
                      << ", " << p.z << ", " << p.w << ")\n";
        }
        return true;
    }

    if (cmd == "assert") {
        std::string a;
        if (!(in >> a)) { std::cout << "  usage: assert ...\n"; return true; }

        if (a == "at") {
            // assert at <name> <x> <y> <z> <w> [eps] — exact camera-space position.
            // Precise enough to distinguish local-frame from world-frame rotation
            // order, which mere left/right/up/down classification cannot.
            std::string name; glm::vec4 want; float eps = 1e-3f;
            if (!(in >> name >> want.x >> want.y >> want.z >> want.w)) {
                std::cout << "  usage: assert at <name> <x> <y> <z> <w> [eps]\n"; return true;
            }
            in >> eps;  // optional
            glm::vec4 p;
            if (!getPoint(name, p)) { ++g_failures; return true; }
            glm::vec4 got = classify(g_camera, p).cam;
            bool pass = glm::length(got - want) <= eps;
            std::cout << (pass ? "  PASS" : "  FAIL") << " " << name << " cam-space ("
                      << got.x << ", " << got.y << ", " << got.z << ", " << got.w << ")"
                      << (pass ? "" : " != expected") << "\n";
            if (!pass) ++g_failures;
            return true;
        }

        if (a == "dist") {
            std::string n1, n2; float expected, eps = 1e-3f;
            glm::vec4 p1, p2;
            if (!(in >> n1 >> n2 >> expected)) { std::cout << "  usage: assert dist <a> <b> <expected> [eps]\n"; return true; }
            in >> eps;  // optional
            if (!getPoint(n1, p1) || !getPoint(n2, p2)) { ++g_failures; return true; }
            float d = glm::length(p1 - p2);
            bool pass = std::abs(d - expected) <= eps;
            std::cout << (pass ? "  PASS" : "  FAIL") << " dist(" << n1 << "," << n2
                      << ") = " << d << " (expected " << expected << " +/- " << eps << ")\n";
            if (!pass) ++g_failures;
            return true;
        }

        // assert <name> <direction>
        std::string dir;
        if (!(in >> dir)) { std::cout << "  usage: assert <name> <direction>\n"; return true; }
        glm::vec4 p;
        if (!getPoint(a, p)) { ++g_failures; return true; }
        Classification c = classify(g_camera, p);
        bool pass = false;
        if      (dir == "left")   pass = c.cam.x < 0;
        else if (dir == "right")  pass = c.cam.x > 0;
        else if (dir == "above")  pass = c.cam.y > 0;
        else if (dir == "below")  pass = c.cam.y < 0;
        else if (dir == "front")  pass = c.inFront;
        else if (dir == "behind") pass = !c.inFront;
        else { std::cout << "  unknown direction '" << dir << "'\n"; return true; }
        std::cout << (pass ? "  PASS" : "  FAIL") << " " << a << " is " << dir
                  << "  [cam-space x=" << c.cam.x << " y=" << c.cam.y << " w=" << c.cam.w << "]\n";
        if (!pass) ++g_failures;
        return true;
    }

    std::cout << "  unknown command '" << cmd << "' (try 'help')\n";
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "cannot open " << argv[1] << "\n";
            return 2;
        }
        std::string line;
        while (std::getline(file, line)) {
            if (!handle(line)) break;
        }
        if (g_failures > 0) {
            std::cout << "\n" << g_failures << " assertion(s) FAILED\n";
            return 1;
        }
        std::cout << "\nall assertions passed\n";
        return 0;
    }

    // Interactive REPL
    std::cout << "4dscene — interactive 4D scene query. Type 'help' or 'quit'.\n";
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (!handle(line)) break;
    }
    return g_failures > 0 ? 1 : 0;
}
