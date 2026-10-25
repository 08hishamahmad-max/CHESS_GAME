// App.cpp
#include "App.h"

#include <algorithm>
#include <iostream>

namespace gui {

namespace {

// Initial window size: the full canvas, or smaller on small screens.
sf::VideoMode initialMode() {
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    float scale = std::min({1.f, 0.92f * static_cast<float>(desktop.width) / layout::WIDTH,
                            0.86f * static_cast<float>(desktop.height) / layout::HEIGHT});
    if (scale <= 0.f) scale = 1.f;  // unknown desktop size (e.g. virtual display)
    return sf::VideoMode(static_cast<unsigned>(layout::WIDTH * scale), static_cast<unsigned>(layout::HEIGHT * scale));
}

sf::ContextSettings contextSettings() {
    sf::ContextSettings s;
    s.antialiasingLevel = 8;  // smooth edges for circles, arrows and rounded shapes
    return s;
}

}  // namespace

App::App(const std::string& assetDir, const std::string& dataDir)
    : window_(initialMode(), "Chess", sf::Style::Default, contextSettings()),
      contextReady_(initContext(assetDir, dataDir)),
      menu_(ctx_),
      game_(ctx_) {
    window_.setFramerateLimit(60);
    view_ = sf::View(sf::FloatRect(0.f, 0.f, layout::WIDTH, layout::HEIGHT));
    updateView();

    sf::Image icon;
    if (icon.loadFromFile(assetDir + "/pieces/wN.png"))
        window_.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());
    cursorsOk_ = arrowCursor_.loadFromSystem(sf::Cursor::Arrow) && handCursor_.loadFromSystem(sf::Cursor::Hand);
}

bool App::initContext(const std::string& assetDir, const std::string& dataDir) {
    ctx_.window = &window_;
    ctx_.assetDir = assetDir;
    ctx_.settingsPath = dataDir + "/chess_settings.cfg";
    ctx_.savesDir = dataDir + "/saves";
    ctx_.settings.load(ctx_.settingsPath);
    ctx_.sounds.setEnabled(ctx_.settings.sound);
    bool ok = ctx_.fonts.load(assetDir);
    if (!ctx_.renderer.loadPieces(assetDir)) ok = false;
    if (!ok) std::cerr << "Warning: some assets are missing in '" << assetDir << "'\n";
    return ok;
}

void App::updateView() {
    // Keep the canvas proportions: add bars left/right or top/bottom.
    const float windowRatio = static_cast<float>(window_.getSize().x) / static_cast<float>(std::max(1u, window_.getSize().y));
    const float canvasRatio = layout::WIDTH / layout::HEIGHT;
    sf::FloatRect viewport(0.f, 0.f, 1.f, 1.f);
    if (windowRatio > canvasRatio) {
        viewport.width = canvasRatio / windowRatio;
        viewport.left = (1.f - viewport.width) / 2.f;
    } else {
        viewport.height = windowRatio / canvasRatio;
        viewport.top = (1.f - viewport.height) / 2.f;
    }
    view_.setViewport(viewport);
    window_.setView(view_);
}

sf::Vector2f App::toCanvas(int x, int y) const { return window_.mapPixelToCoords({x, y}, view_); }

int App::run() {
    sf::Clock frameClock;
    while (window_.isOpen()) {
        sf::Event event;
        while (window_.pollEvent(event)) {
            switch (event.type) {
                case sf::Event::Closed:
                    window_.close();
                    continue;
                case sf::Event::Resized:
                    updateView();
                    continue;
                case sf::Event::MouseMoved:
                    mouse_ = toCanvas(event.mouseMove.x, event.mouseMove.y);
                    break;
                case sf::Event::MouseButtonPressed:
                case sf::Event::MouseButtonReleased:
                    mouse_ = toCanvas(event.mouseButton.x, event.mouseButton.y);
                    break;
                default:
                    break;
            }
            if (screen_ == Screen::Menu)
                menu_.handleEvent(event, mouse_);
            else
                game_.handleEvent(event, mouse_);
        }

        // Screen changes requested by the screens.
        if (screen_ == Screen::Menu) {
            ScreenRequest r = menu_.takeRequest();
            if (r == ScreenRequest::StartGame) {
                game_.start(menu_.config());
                screen_ = Screen::Game;
            } else if (r == ScreenRequest::Quit) {
                window_.close();
            }
        } else if (game_.takeRequest() == ScreenRequest::OpenMenu) {
            screen_ = Screen::Menu;
        }

        const float dt = frameClock.restart().asSeconds();
        if (screen_ == Screen::Game) game_.update(dt);

        // Hand cursor over clickable things.
        bool hand = screen_ == Screen::Menu ? menu_.wantsHandCursor(mouse_) : game_.wantsHandCursor(mouse_);
        if (cursorsOk_ && hand != handShown_) {
            window_.setMouseCursor(hand ? handCursor_ : arrowCursor_);
            handShown_ = hand;
        }

        window_.clear(sf::Color(24, 23, 21));  // colour of the letter-box bars
        sf::RectangleShape background({layout::WIDTH, layout::HEIGHT});
        background.setFillColor(palette::Background);
        window_.draw(background);
        if (screen_ == Screen::Menu)
            menu_.draw(window_, mouse_);
        else
            game_.draw(window_, mouse_);
        window_.display();
    }
    ctx_.saveSettings();
    return 0;
}

}  // namespace gui
