#include "SaveGame.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "third_party/json.hpp"

using nlohmann::json;

namespace {

// Encode a flat map of SaveValue as a JSON object. The three variant types map
// 1:1 onto distinct JSON types (number / boolean / string), so decode can
// dispatch on the JSON type alone — no tag wrapper needed.
json encodeMap(const std::map<std::string, SaveValue>& m) {
    json o = json::object();
    for (const auto& [k, v] : m) {
        std::visit([&](auto&& x) { o[k] = x; }, v);
    }
    return o;
}

// Decode a JSON object back into a flat SaveValue map. Booleans must be tested
// before numbers (a JSON bool also answers is_number() in some paths). Unknown
// value types (arrays, objects, null) are skipped for forward-compatibility.
std::map<std::string, SaveValue> decodeMap(const json& o) {
    std::map<std::string, SaveValue> m;
    if (!o.is_object()) return m;
    for (auto it = o.begin(); it != o.end(); ++it) {
        const json& v = it.value();
        if (v.is_boolean())      m[it.key()] = v.get<bool>();
        else if (v.is_number())  m[it.key()] = v.get<double>();
        else if (v.is_string())  m[it.key()] = v.get<std::string>();
        // else: skip (additive forward-compat)
    }
    return m;
}

// $HOME (or %USERPROFILE% on Windows), or "" if unset.
std::string homeDir() {
    if (const char* h = std::getenv("HOME")) return h;
#ifdef _WIN32
    if (const char* u = std::getenv("USERPROFILE")) return u;
#endif
    return {};
}

}  // namespace

bool loadSaveGame(const std::string& path, SaveGame& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    try {
        json root = json::parse(ss.str());
        SaveGame sg;
        sg.version = root.value("version", 0);
        if (sg.version != SaveGame::kSchemaVersion) return false;  // treat as no save
        sg.currentScene = root.value("currentScene", std::string());
        if (root.contains("shared")) sg.shared = decodeMap(root["shared"]);
        if (root.contains("scenes") && root["scenes"].is_object()) {
            for (auto it = root["scenes"].begin(); it != root["scenes"].end(); ++it)
                sg.sceneBlobs[it.key()] = decodeMap(it.value());
        }
        out = std::move(sg);
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[SaveGame] ignoring unreadable save '%s': %s\n",
                     path.c_str(), e.what());
        return false;
    }
}

bool writeSaveGame(const std::string& path, const SaveGame& sg) {
    std::error_code ec;
    std::filesystem::path p(path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path(), ec);  // ec ignored; open will fail if it mattered

    json root;
    root["version"]      = sg.version;
    root["currentScene"] = sg.currentScene;
    root["shared"]       = encodeMap(sg.shared);
    json scenes = json::object();
    for (const auto& [name, blob] : sg.sceneBlobs)
        scenes[name] = encodeMap(blob);
    root["scenes"] = scenes;

    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << root.dump(2);
    return (bool)f;
}

void eraseSaveGame(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

bool saveExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::string defaultSavePath() {
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA"))
        return std::string(appdata) + "\\elua\\save.json";
    std::string home = homeDir();
    return home.empty() ? "save.json" : home + "\\elua_save.json";
#elif defined(__APPLE__)
    std::string home = homeDir();
    if (!home.empty()) return home + "/Library/Application Support/elua/save.json";
    return "save.json";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"))
        if (xdg[0] != '\0') return std::string(xdg) + "/elua/save.json";
    std::string home = homeDir();
    if (!home.empty()) return home + "/.local/share/elua/save.json";
    return "save.json";
#endif
}
