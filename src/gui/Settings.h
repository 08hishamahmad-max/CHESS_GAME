// Settings.h - user preferences, saved to a small text file
// ("chess_settings.cfg", one "key=value" per line) next to the program.
#pragma once

#include <string>

namespace gui {

struct Settings {
    // Board & interface
    bool showLegalMoves = true;
    bool showCoordinates = true;
    bool animations = true;
    bool sound = true;
    bool autoQueen = false;        // promote to a queen without asking
    bool autoFlip = false;         // two-player mode: turn the board after each move
    int boardTheme = 0;            // index into boardThemes()

    // Last choices in the start menu (remembered for convenience)
    int mode = 1;                  // 0 = two players, 1 = vs computer
    int humanColor = 0;            // 0 = white, 1 = black, 2 = random
    int aiLevel = 3;               // 1..5
    int timeBase = 0;              // seconds, 0 = no clock
    int timeIncrement = 0;         // seconds
    std::string playerName = "Player";

    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace gui
