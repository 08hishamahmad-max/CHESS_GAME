// AppContext.h - things shared by all screens (window, fonts, piece images,
// sounds, settings), plus the GameConfig the menu hands to the game screen.
#pragma once

#include "Renderer.h"
#include "Settings.h"
#include "SoundManager.h"
#include "Ui.h"
#include "engine/ChessClock.h"
#include "engine/Types.h"

#include <SFML/Graphics.hpp>

#include <string>

namespace gui {

struct AppContext {
    sf::RenderWindow* window = nullptr;
    Fonts fonts;
    Renderer renderer;
    SoundManager sounds;
    Settings settings;
    std::string assetDir;
    std::string settingsPath = "chess_settings.cfg";
    std::string savesDir = "saves";

    void saveSettings() { settings.save(settingsPath); }
    const BoardTheme& theme() const {
        const auto& themes = boardThemes();
        return themes[static_cast<std::size_t>(settings.boardTheme) % themes.size()];
    }
};

// Everything needed to start a game.
struct GameConfig {
    bool vsComputer = true;
    chess::Color humanColor = chess::Color::White;  // only used vs computer
    bool randomColor = false;
    int aiLevel = 3;
    chess::TimeControl timeControl;  // baseSeconds == 0 -> no clock
    std::string startFen;            // empty = normal start position
    std::string pgn;                 // non-empty = load this game
};

// Requests a screen makes to the App.
enum class ScreenRequest { None, StartGame, OpenMenu, Quit };

}  // namespace gui
