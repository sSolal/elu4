#pragma once

#include "GameState.h"

class Menu {
public:
    Menu();
    ~Menu();

    // Returns new state if user clicked a button, or current state if nothing clicked
    GameState renderMainMenu(GameState current);
    GameState renderBackButton(GameState current);

private:
    bool initialized = false;
};
