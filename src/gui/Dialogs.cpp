// Dialogs.cpp
#include "Dialogs.h"

#include <SFML/Window/Clipboard.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace gui {

namespace {
constexpr float PADDING = 26.f;
constexpr float TITLE_SIZE = 24.f;
constexpr float LINE_H = 24.f;
constexpr float BUTTON_H = 42.f;
constexpr float GAP = 10.f;
}  // namespace

Modal::Modal(std::string title, std::vector<std::string> lines)
    : title_(std::move(title)), lines_(std::move(lines)), open_(true) {}

void Modal::addButton(int id, const std::string& label, bool primary) {
    Button b;
    b.id = id;
    b.label = label;
    b.primary = primary;
    buttons_.push_back(b);
}

void Modal::enableTextField(const std::string& initialText, const std::string& placeholder) {
    hasTextField_ = true;
    text_ = initialText;
    placeholder_ = placeholder;
}

void Modal::layout(bool verticalButtons, float width) {
    float height = PADDING + TITLE_SIZE + 18.f + LINE_H * static_cast<float>(lines_.size());
    if (hasTextField_) height += 54.f + LINE_H;  // field + error line
    const std::size_t n = buttons_.size();
    if (verticalButtons)
        height += 10.f + static_cast<float>(n) * (BUTTON_H + GAP);
    else if (n > 0)
        height += 16.f + BUTTON_H + GAP;
    height += PADDING - GAP;

    rect_ = {(layout::WIDTH - width) / 2.f, (layout::HEIGHT - height) / 2.f, width, height};

    float y = rect_.top + PADDING + TITLE_SIZE + 18.f + LINE_H * static_cast<float>(lines_.size());
    if (hasTextField_) {
        fieldRect_ = {rect_.left + PADDING, y + 6.f, width - 2 * PADDING, 42.f};
        y += 54.f + LINE_H;
    }
    const float innerW = width - 2 * PADDING;
    if (verticalButtons) {
        y += 10.f;
        for (Button& b : buttons_) {
            b.rect = {rect_.left + PADDING, y, innerW, BUTTON_H};
            y += BUTTON_H + GAP;
        }
    } else if (n > 0) {
        y += 16.f;
        const float bw = (innerW - GAP * static_cast<float>(n - 1)) / static_cast<float>(n);
        for (std::size_t i = 0; i < n; ++i)
            buttons_[i].rect = {rect_.left + PADDING + static_cast<float>(i) * (bw + GAP), y, bw, BUTTON_H};
    }
    open_ = true;
}

void Modal::draw(sf::RenderTarget& target, const Fonts& fonts, sf::Vector2f mouse) const {
    if (!open_) return;
    // Dim everything behind the dialog.
    sf::RectangleShape dim({layout::WIDTH, layout::HEIGHT});
    dim.setFillColor(sf::Color(0, 0, 0, 150));
    target.draw(dim);

    sf::FloatRect shadow = rect_;
    shadow.top += 6.f;
    drawRoundedRect(target, shadow, 14.f, sf::Color(0, 0, 0, 110));
    drawRoundedRect(target, rect_, 14.f, palette::Panel);
    drawRoundedRect(target, {rect_.left, rect_.top, rect_.width, 6.f}, 3.f, accent);

    float y = rect_.top + PADDING;
    drawText(target, fonts, title_, {rect_.left + rect_.width / 2.f, y - 4.f}, static_cast<unsigned>(TITLE_SIZE),
             palette::Text, Weight::Bold, Align::Center);
    y += TITLE_SIZE + 18.f;
    for (const std::string& line : lines_) {
        drawText(target, fonts, line, {rect_.left + rect_.width / 2.f, y}, 16, palette::TextDim, Weight::Regular,
                 Align::Center);
        y += LINE_H;
    }

    if (hasTextField_) {
        drawRoundedRect(target, fieldRect_, 7.f, palette::PanelDark, palette::Accent, 1.5f);
        // Show the end of the text if it is too long for the box.
        std::string shown = text_;
        const unsigned size = 15;
        while (!shown.empty() && textWidth(fonts, shown, size) > fieldRect_.width - 24.f) shown.erase(0, 1);
        if (shown.empty() && text_.empty()) {
            drawText(target, fonts, placeholder_, {fieldRect_.left + 12.f, fieldRect_.top + 11.f}, size, palette::TextFaint);
        } else {
            float w = drawText(target, fonts, shown, {fieldRect_.left + 12.f, fieldRect_.top + 11.f}, size, palette::Text);
            if (static_cast<int>(caretClock_.getElapsedTime().asSeconds() * 2.f) % 2 == 0) {
                sf::RectangleShape caret({2.f, 22.f});
                caret.setPosition(fieldRect_.left + 14.f + w, fieldRect_.top + 10.f);
                caret.setFillColor(palette::Text);
                target.draw(caret);
            }
        }
        if (text_.empty()) {
            sf::RectangleShape caret({2.f, 22.f});
            caret.setPosition(fieldRect_.left + 12.f, fieldRect_.top + 10.f);
            caret.setFillColor(sf::Color(240, 238, 234, 120));
            target.draw(caret);
        }
        if (!error_.empty())
            drawText(target, fonts, error_, {rect_.left + rect_.width / 2.f, fieldRect_.top + fieldRect_.height + 6.f}, 14,
                     palette::Danger, Weight::SemiBold, Align::Center);
    }

    for (const Button& b : buttons_) b.draw(target, fonts, mouse);
}

int Modal::onClick(sf::Vector2f pos) {
    if (!open_) return -1;
    return hitTest(buttons_, pos);
}

int Modal::onKey(const sf::Event::KeyEvent& key) {
    if (!open_) return -1;
    if (key.code == sf::Keyboard::Enter) return defaultId;
    if (key.code == sf::Keyboard::Escape) return cancelId;
    if (hasTextField_ && key.code == sf::Keyboard::V && (key.control || key.system)) {
        // Paste from the clipboard (newlines become spaces).
        std::string clip = sf::Clipboard::getString().toAnsiString();
        for (char c : clip) {
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
            if (text_.size() < 400) text_ += c;
        }
        error_.clear();
    }
    return -1;
}

void Modal::onText(sf::Uint32 unicode) {
    if (!open_ || !hasTextField_) return;
    if (unicode == 8) {  // backspace
        if (!text_.empty()) text_.pop_back();
    } else if (unicode >= 32 && unicode < 127 && text_.size() < 400) {
        text_ += static_cast<char>(unicode);
    }
    error_.clear();
}

// ---------------------------------------------------------------------------
// Settings dialog
// ---------------------------------------------------------------------------
namespace settings_dialog {

Modal make(const Settings& s) {
    auto onOff = [](bool b) { return std::string(b ? "On" : "Off"); };
    Modal m("Settings");
    m.addButton(FIRST_ID + 0, "Show legal moves: " + onOff(s.showLegalMoves));
    m.addButton(FIRST_ID + 1, "Board coordinates: " + onOff(s.showCoordinates));
    m.addButton(FIRST_ID + 2, "Piece animation: " + onOff(s.animations));
    m.addButton(FIRST_ID + 3, "Sound effects: " + onOff(s.sound));
    m.addButton(FIRST_ID + 4, "Auto-queen on promotion: " + onOff(s.autoQueen));
    m.addButton(FIRST_ID + 5, "Auto-flip board (2 players): " + onOff(s.autoFlip));
    const auto& themes = boardThemes();
    m.addButton(FIRST_ID + 6, std::string("Board theme: ") +
                                  themes[static_cast<std::size_t>(s.boardTheme) % themes.size()].name);
    m.addButton(CLOSE_ID, "Done", true);
    // Highlight the options that are switched on.
    bool flags[6] = {s.showLegalMoves, s.showCoordinates, s.animations, s.sound, s.autoQueen, s.autoFlip};
    for (int i = 0; i < 6; ++i) m.buttons()[static_cast<std::size_t>(i)].selected = flags[i];
    m.defaultId = CLOSE_ID;
    m.cancelId = CLOSE_ID;
    m.layout(true, 420.f);
    return m;
}

bool apply(Settings& s, int id) {
    switch (id - FIRST_ID) {
        case 0: s.showLegalMoves = !s.showLegalMoves; return true;
        case 1: s.showCoordinates = !s.showCoordinates; return true;
        case 2: s.animations = !s.animations; return true;
        case 3: s.sound = !s.sound; return true;
        case 4: s.autoQueen = !s.autoQueen; return true;
        case 5: s.autoFlip = !s.autoFlip; return true;
        case 6: s.boardTheme = (s.boardTheme + 1) % static_cast<int>(boardThemes().size()); return true;
        default: return false;
    }
}

}  // namespace settings_dialog

// ---------------------------------------------------------------------------
// Files and clipboard
// ---------------------------------------------------------------------------
std::vector<std::string> listPgnFiles(const std::string& dir, std::size_t max) {
    namespace fs = std::filesystem;
    std::vector<std::pair<fs::file_time_type, std::string>> found;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return {};
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".pgn") found.emplace_back(entry.last_write_time(ec), entry.path().filename().string());
    }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<std::string> names;
    for (std::size_t i = 0; i < found.size() && i < max; ++i) names.push_back(found[i].second);
    return names;
}

bool readTextFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool writeTextFile(const std::string& path, const std::string& text) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path p(path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

std::string clipboardText() {
    sf::String s = sf::Clipboard::getString();
    std::basic_string<sf::Uint8> utf8 = s.toUtf8();
    return std::string(utf8.begin(), utf8.end());
}

}  // namespace gui
