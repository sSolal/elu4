#pragma once

class Menu {
public:
    Menu();
    ~Menu();

    // Renders the level list. Returns the index of the clicked level, or -1 if
    // nothing was clicked this frame.
    int renderMainMenu();

    // Renders the in-level Back button. Returns true if it was clicked.
    bool renderBackButton();

private:
    bool initialized = false;
};
