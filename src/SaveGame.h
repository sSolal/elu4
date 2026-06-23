#pragma once

#include <map>
#include <string>
#include <variant>

// Persistent game state. Two stores: a flat cross-scene `shared` table (the
// "which corridor did I come through" entry handshake + story flags like
// intro_done) and per-scene private `sceneBlobs` filled by each scene's optional
// Lua save() hook. Values are restricted to the three Lua-friendly scalar types
// so the JSON encode/decode and the sol2 round-trip stay total and trivial — a
// scene that needs to persist a position stores four numbers, not a vec4.
using SaveValue = std::variant<double, bool, std::string>;

struct SaveGame {
    // Bumped only on an incompatible change; a mismatched file is treated as
    // "no save" (clean slate). The decode-by-JSON-type scheme already tolerates
    // additive key changes without a version bump.
    static constexpr int kSchemaVersion = 1;

    int         version = kSchemaVersion;
    std::string currentScene;                  // scene to resume on "Continue"
    std::map<std::string, SaveValue> shared;   // cross-scene flat store
    std::map<std::string, std::map<std::string, SaveValue>> sceneBlobs;  // per scene
};

// Read the save file at `path` into `out`. Returns false (leaving `out` default)
// if the file is missing, unreadable, malformed, or a different schema version —
// callers treat any false as "no save exists", never an error.
bool loadSaveGame(const std::string& path, SaveGame& out);

// Serialize `sg` to `path` as JSON, creating the parent directory if needed.
// Returns false on an I/O failure.
bool writeSaveGame(const std::string& path, const SaveGame& sg);

// Delete the save file (no-op if it doesn't exist).
void eraseSaveGame(const std::string& path);

// True if a readable file exists at `path` (does not validate its contents).
bool saveExists(const std::string& path);

// The per-user save-file location, picked so it works from a read-only AppImage
// or a packaged build: $XDG_DATA_HOME/elua (or ~/.local/share/elua) on Linux,
// %APPDATA%\elua on Windows, ~/Library/Application Support/elua on macOS.
std::string defaultSavePath();
