// Dialogs.h - a generic modal dialog: title, message lines, an optional
// one-line text field and a set of buttons. Used for the game-over screen,
// settings, draw offers, loading games, entering a FEN, etc.
#pragma once

#include "Settings.h"
#include "Ui.h"

#include <SFML/Graphics.hpp>

#include <string>
#include <vector>

namespace gui {

class Modal {
public:
    // Builds a dialog. Buttons are added with addButton(); call layout() after.
    Modal() = default;
    Modal(std::string title, std::vector<std::string> lines = {});

    void addButton(int id, const std::string& label, bool primary = false);
    void enableTextField(const std::string& initialText, const std::string& placeholder);

    // Positions everything. `verticalButtons` stacks buttons full-width.
    void layout(bool verticalButtons = false, float width = 460.f);

    void draw(sf::RenderTarget& target, const Fonts& fonts, sf::Vector2f mouse) const;

    // Input. Each returns the id of the activated button, or -1.
    int onClick(sf::Vector2f pos);
    int onKey(const sf::Event::KeyEvent& key);  // Enter -> defaultId, Escape -> cancelId
    void onText(sf::Uint32 unicode);

    bool isOpen() const { return open_; }
    void close() { open_ = false; }

    const std::string& text() const { return text_; }
    void setError(const std::string& error) { error_ = error; }
    void setLines(std::vector<std::string> lines) { lines_ = std::move(lines); }
    std::vector<Button>& buttons() { return buttons_; }
    const std::vector<Button>& buttons() const { return buttons_; }

    int defaultId = -1;  // button activated by Enter
    int cancelId = -1;   // button activated by Escape
    sf::Color accent = palette::Accent;  // colour of the title bar strip

private:
    std::string title_;
    std::vector<std::string> lines_;
    std::vector<Button> buttons_;
    bool hasTextField_ = false;
    std::string text_, placeholder_, error_;
    sf::FloatRect rect_, fieldRect_;
    bool open_ = false;
    sf::Clock caretClock_;
};

// ---------------------------------------------------------------------------
// Ready-made dialogs and file helpers shared by the menu and the game screen
// ---------------------------------------------------------------------------
namespace settings_dialog {
constexpr int FIRST_ID = 300;
constexpr int CLOSE_ID = 399;
Modal make(const Settings& s);
// Applies a click on a settings button. Returns true if `id` belonged to it.
bool apply(Settings& s, int id);
}  // namespace settings_dialog

// Up to `max` .pgn files in `dir`, newest first (file names only).
std::vector<std::string> listPgnFiles(const std::string& dir, std::size_t max = 8);
bool readTextFile(const std::string& path, std::string& out);
bool writeTextFile(const std::string& path, const std::string& text);
// Clipboard text as UTF-8 / plain string.
std::string clipboardText();

}  // namespace gui
