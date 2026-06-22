#pragma once

class Menu {
public:
    Menu();
    ~Menu();

    // Sentinel returned by renderMainMenu() when the player clicks "Start game"
    // (distinct from a level index, which is >= 0).
    static constexpr int kStartGame = -2;

    // Renders the Elua title screen: animated lamplit background, the "Elua"
    // wordmark + logo mark + tagline, the level panel, and the Credits entry.
    // Returns the index of the clicked level, or -1 if nothing was picked this
    // frame (including while the Credits overlay is showing).
    int renderMainMenu();

    // Renders the in-level Back button. Returns true if it was clicked.
    bool renderBackButton();

    // Renders the in-level Settings button (below Back). Returns true if clicked.
    bool renderSettingsButton();

private:
    // The Credits overlay replaces the level panel when set; toggled by the
    // Credits button and cleared by its Back button.
    bool showCredits = false;

    // Draws the wordmark + mark + taglines via the window draw list. Returns the
    // y (window-local) just below the title block, where content begins.
    float drawTitle(float winW, float winH, float t);
    // The frosted level list + Credits button + footer. Returns picked index/-1.
    int drawLevelPanel(float winW, float winH, float contentTop);
    // The frosted credits card with a Back button.
    void drawCredits(float winW, float winH, float contentTop);
};
