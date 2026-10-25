// App.h - owns the window and the screens (menu, game) and runs the main loop.
//
// The whole interface is drawn on a fixed "virtual" canvas
// (layout::WIDTH x layout::HEIGHT). When the window is resized the canvas is
// scaled to fit, keeping its proportions (letter-boxing), so the layout never
// breaks.
#pragma once

#include "AppContext.h"
#include "GameScreen.h"
#include "MenuScreen.h"

#include <SFML/Graphics.hpp>

#include <string>

namespace gui {

class App {
public:
    // `assetDir`: folder with fonts/ and pieces/. `dataDir`: where settings
    // and saved games are stored (normally the folder of the executable).
    App(const std::string& assetDir, const std::string& dataDir);
    int run();

private:
    bool initContext(const std::string& assetDir, const std::string& dataDir);
    void updateView();
    sf::Vector2f toCanvas(int x, int y) const;

    sf::RenderWindow window_;
    AppContext ctx_;
    bool contextReady_;  // initialised before the screens (they read the settings)
    MenuScreen menu_;
    GameScreen game_;
    enum class Screen { Menu, Game } screen_ = Screen::Menu;

    sf::View view_;
    sf::Vector2f mouse_;
    sf::Cursor arrowCursor_, handCursor_;
    bool cursorsOk_ = false;
    bool handShown_ = false;
};

}  // namespace gui
