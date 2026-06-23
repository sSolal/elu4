#pragma once

class Menu {
public:
    Menu();
    ~Menu();

    // Sentinels returned by renderMainMenu() when the player clicks a primary
    // action (all distinct from a level index, which is >= 0). kStartGame: begin a
    // fresh game (shown when no save exists). kContinueGame: resume the save.
    // kRestartGame: erase the save and begin fresh (shown only with a save, after
    // the player confirms the prompt).
    static constexpr int kStartGame    = -2;
    static constexpr int kContinueGame = -3;
    static constexpr int kRestartGame  = -4;

    // Renders the Elua title screen: animated lamplit background, the "Elua"
    // wordmark + logo mark + tagline, the level panel, and the Credits entry.
    // `hasSave` switches the primary button to "Continue game" and reveals the
    // subtle "Restart game" option. Returns the index of the clicked level, a
    // sentinel above, or -1 if nothing was picked this frame.
    int renderMainMenu(bool hasSave);

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
    // The frosted level list + Credits button + footer. Returns picked index/-1
    // (or a primary-action sentinel). `hasSave` toggles Continue/Restart.
    int drawLevelPanel(float winW, float winH, float contentTop, bool hasSave);
    // The frosted credits card with a Back button.
    void drawCredits(float winW, float winH, float contentTop);
};
