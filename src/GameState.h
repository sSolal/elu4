#pragma once

// Top-level app state. Which specific level is active is tracked separately as an
// index into levelRegistry(), so adding levels never touches this enum.
enum class GameState { MENU, IN_LEVEL };
