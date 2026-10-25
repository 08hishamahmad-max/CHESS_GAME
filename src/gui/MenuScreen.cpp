// MenuScreen.cpp
#include "MenuScreen.h"

#include "engine/AiLevel.h"
#include "engine/Game.h"
#include "engine/Pgn.h"

#include <cstdio>

using namespace chess;

namespace gui {

namespace {

struct TimePreset {
    int base, inc;
    const char* label;
};
const TimePreset PRESETS[6] = {{0, 0, "No clock"}, {60, 0, "1+0"}, {180, 2, "3+2"},
                               {300, 0, "5+0"},    {600, 0, "10+0"}, {900, 10, "15+10"}};

// Parses "7+3", "7 3" or "7" (minutes + increment seconds).
bool parseTimeControl(std::string text, TimeControl& tc) {
    for (char& c : text)
        if (c < '0' || c > '9') c = ' ';  // "7+3" -> "7 3"
    int minutes = 0, inc = 0;
    int n = std::sscanf(text.c_str(), "%d %d", &minutes, &inc);
    if (n < 1) return false;
    if (n == 1) inc = 0;
    if (minutes < 1 || minutes > 180 || inc < 0 || inc > 180) return false;
    tc.baseSeconds = minutes * 60;
    tc.incrementSeconds = inc;
    return true;
}

}  // namespace

MenuScreen::MenuScreen(AppContext& ctx) : ctx_(ctx) {
    const Settings& s = ctx_.settings;
    config_.vsComputer = s.mode == 1;
    config_.humanColor = s.humanColor == 1 ? Color::Black : Color::White;
    config_.randomColor = s.humanColor == 2;
    config_.aiLevel = s.aiLevel;
    config_.timeControl = {s.timeBase, s.timeIncrement};
    rebuild();
}

ScreenRequest MenuScreen::takeRequest() {
    ScreenRequest r = request_;
    request_ = ScreenRequest::None;
    return r;
}

void MenuScreen::rebuild() {
    buttons_.clear();
    const float x = layout::PANEL_X + 24.f, w = layout::PANEL_W - 48.f, h = 44.f, gap = 10.f;
    auto row = [&](float y, std::initializer_list<std::pair<int, std::string>> items) {
        const float n = static_cast<float>(items.size());
        const float bw = (w - gap * (n - 1.f)) / n;
        float bx = x;
        for (const auto& it : items) {
            Button b;
            b.id = it.first;
            b.label = it.second;
            b.rect = {bx, y, bw, h};
            buttons_.push_back(b);
            bx += bw + gap;
        }
    };
    auto find = [&](int id) -> Button& {
        for (Button& b : buttons_)
            if (b.id == id) return b;
        return buttons_.front();
    };

    row(136.f, {{OppFriend, "Friend (2 players)"}, {OppComputer, "Computer"}});
    row(220.f, {{ColorWhite, "White"}, {ColorRandom, "Random"}, {ColorBlack, "Black"}});
    row(304.f, {{Level1, "1"}, {Level2, "2"}, {Level3, "3"}, {Level4, "4"}, {Level5, "5"}});
    row(388.f, {{Time0, PRESETS[0].label}, {Time1, PRESETS[1].label}, {Time2, PRESETS[2].label}, {Time3, PRESETS[3].label}});
    row(442.f, {{Time4, PRESETS[4].label}, {Time5, PRESETS[5].label}, {TimeCustom, "Custom..."}});
    row(588.f, {{LoadPgn, "Load PGN"}, {PastePosition, "Paste FEN / PGN"}});
    row(642.f, {{OpenSettings, "Settings"}, {Quit, "Quit"}});
    Button start;
    start.id = Start;
    start.label = "Start game";
    start.primary = true;
    start.fontSize = 20;
    start.rect = {x, 512.f, w, 58.f};
    buttons_.push_back(start);

    // Show the current choices as selected.
    find(config_.vsComputer ? OppComputer : OppFriend).selected = true;
    for (int id : {ColorWhite, ColorRandom, ColorBlack}) find(id).enabled = config_.vsComputer;
    for (int id = Level1; id <= Level5; ++id) find(id).enabled = config_.vsComputer;
    if (config_.vsComputer) {
        find(config_.randomColor ? ColorRandom : (config_.humanColor == Color::White ? ColorWhite : ColorBlack)).selected = true;
        find(Level1 + config_.aiLevel - 1).selected = true;
    }
    bool preset = false;
    for (int i = 0; i < 6; ++i) {
        if (PRESETS[i].base == config_.timeControl.baseSeconds &&
            (PRESETS[i].base == 0 || PRESETS[i].inc == config_.timeControl.incrementSeconds)) {
            find(Time0 + i).selected = true;
            preset = true;
        }
    }
    if (!preset) {
        Button& c = find(TimeCustom);
        c.selected = true;
        c.label = "Custom " + config_.timeControl.toString();
    }
}

void MenuScreen::onButton(int id) {
    ctx_.sounds.play(SoundEffect::Move);
    Settings& s = ctx_.settings;
    if (id == OppFriend || id == OppComputer) {
        config_.vsComputer = id == OppComputer;
    } else if (id == ColorWhite || id == ColorBlack || id == ColorRandom) {
        config_.randomColor = id == ColorRandom;
        config_.humanColor = id == ColorBlack ? Color::Black : Color::White;
    } else if (id >= Level1 && id <= Level5) {
        config_.aiLevel = id - Level1 + 1;
    } else if (id >= Time0 && id <= Time5) {
        config_.timeControl = {PRESETS[id - Time0].base, PRESETS[id - Time0].inc};
    } else if (id == TimeCustom) {
        modal_ = Modal("Custom time control", {"Minutes per player + increment in seconds", "(for example 7+3 or 20+0)"});
        modal_.enableTextField(config_.timeControl.enabled() ? config_.timeControl.toString() : "", "e.g. 7+3");
        modal_.addButton(DlgCancel, "Cancel");
        modal_.addButton(DlgOk, "Set", true);
        modal_.defaultId = DlgOk;
        modal_.cancelId = DlgCancel;
        modal_.layout();
        dialog_ = Dialog::CustomTime;
    } else if (id == Start) {
        startWith("", "");
    } else if (id == LoadPgn) {
        openLoadDialog();
    } else if (id == PastePosition) {
        modal_ = Modal("Load a position or game", {"Paste a FEN string or a PGN game (Ctrl+V)"});
        std::string clip = clipboardText();
        for (char& c : clip)
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        modal_.enableTextField(clip.substr(0, 400), "FEN or PGN");
        modal_.addButton(DlgCancel, "Cancel");
        modal_.addButton(DlgOk, "Load", true);
        modal_.defaultId = DlgOk;
        modal_.cancelId = DlgCancel;
        modal_.layout(false, 640.f);
        dialog_ = Dialog::Position;
    } else if (id == OpenSettings) {
        openSettingsDialog();
    } else if (id == Quit) {
        request_ = ScreenRequest::Quit;
    }
    // Remember the choices for next time.
    s.mode = config_.vsComputer ? 1 : 0;
    s.humanColor = config_.randomColor ? 2 : (config_.humanColor == Color::Black ? 1 : 0);
    s.aiLevel = config_.aiLevel;
    s.timeBase = config_.timeControl.baseSeconds;
    s.timeIncrement = config_.timeControl.incrementSeconds;
    ctx_.saveSettings();
    rebuild();
}

void MenuScreen::openLoadDialog() {
    files_ = listPgnFiles(ctx_.savesDir);
    modal_ = Modal("Load a saved game",
                   files_.empty() ? std::vector<std::string>{"No saved games in the \"" + ctx_.savesDir + "\" folder yet."}
                                  : std::vector<std::string>{"Games saved with \"Save PGN\" (newest first)"});
    for (std::size_t i = 0; i < files_.size(); ++i) modal_.addButton(DlgFileBase + static_cast<int>(i), files_[i]);
    modal_.addButton(DlgCancel, "Cancel");
    modal_.cancelId = DlgCancel;
    modal_.layout(true, 520.f);
    dialog_ = Dialog::Load;
}

void MenuScreen::openSettingsDialog() {
    modal_ = settings_dialog::make(ctx_.settings);
    dialog_ = Dialog::Settings;
}

void MenuScreen::startWith(const std::string& fen, const std::string& pgn) {
    config_.startFen = fen;
    config_.pgn = pgn;
    request_ = ScreenRequest::StartGame;
}

void MenuScreen::onDialogButton(int id) {
    if (id < 0) return;
    if (id == DlgCancel || id == settings_dialog::CLOSE_ID) {
        modal_.close();
        dialog_ = Dialog::None;
        return;
    }
    switch (dialog_) {
        case Dialog::CustomTime: {
            TimeControl tc;
            if (!parseTimeControl(modal_.text(), tc)) {
                modal_.setError("Please enter minutes+increment, e.g. 7+3 (1-180 minutes)");
                return;
            }
            config_.timeControl = tc;
            ctx_.settings.timeBase = tc.baseSeconds;
            ctx_.settings.timeIncrement = tc.incrementSeconds;
            ctx_.saveSettings();
            break;
        }
        case Dialog::Position: {
            const std::string text = modal_.text();
            Board test;
            Game g;
            std::string err;
            if (test.loadFen(text)) {
                startWith(text, "");
            } else if (pgn::read(text, g, nullptr, err)) {
                startWith("", text);
            } else {
                modal_.setError("Not a valid FEN or PGN" + (err.empty() ? "" : ": " + err));
                return;
            }
            break;
        }
        case Dialog::Load: {
            std::size_t index = static_cast<std::size_t>(id - DlgFileBase);
            std::string text;
            Game g;
            std::string err;
            if (index < files_.size() && readTextFile(ctx_.savesDir + "/" + files_[index], text) &&
                pgn::read(text, g, nullptr, err)) {
                startWith("", text);
            } else {
                modal_.setLines({"Could not read that file:", err.empty() ? "file not readable" : err});
                return;
            }
            break;
        }
        case Dialog::Settings:
            if (settings_dialog::apply(ctx_.settings, id)) {
                ctx_.sounds.setEnabled(ctx_.settings.sound);
                ctx_.saveSettings();
                modal_ = settings_dialog::make(ctx_.settings);
            }
            return;
        default:
            break;
    }
    modal_.close();
    dialog_ = Dialog::None;
    rebuild();
}

void MenuScreen::handleEvent(const sf::Event& event, sf::Vector2f mouse) {
    if (dialog_ != Dialog::None) {
        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left)
            onDialogButton(modal_.onClick(mouse));
        else if (event.type == sf::Event::KeyPressed)
            onDialogButton(modal_.onKey(event.key));
        else if (event.type == sf::Event::TextEntered)
            modal_.onText(event.text.unicode);
        return;
    }
    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        int id = hitTest(buttons_, mouse);
        if (id >= 0) onButton(id);
    } else if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Space) onButton(Start);
        if (event.key.code == sf::Keyboard::Escape) request_ = ScreenRequest::Quit;
    }
}

bool MenuScreen::wantsHandCursor(sf::Vector2f mouse) const {
    if (dialog_ != Dialog::None) return false;
    return hitTest(buttons_, mouse) >= 0;
}

void MenuScreen::draw(sf::RenderTarget& target, sf::Vector2f mouse) {
    const Fonts& f = ctx_.fonts;

    // Left: a board with the starting position as decoration.
    BoardState state;
    state.board = &decorBoard_;
    state.theme = &ctx_.theme();
    state.showCoordinates = ctx_.settings.showCoordinates;
    ctx_.renderer.drawBoard(target, f, state);
    drawText(target, f, "Chess", {layout::BOARD_X, 10.f}, 28, palette::Text, Weight::Bold);
    drawText(target, f, "C++17  \xC2\xB7  SFML", {layout::BOARD_X + layout::BOARD_SIZE, 20.f}, 15, palette::TextDim,
             Weight::Regular, Align::Right);
    drawText(target, f, "Full rules  \xC2\xB7  5 computer levels  \xC2\xB7  clocks  \xC2\xB7  PGN / FEN",
             {layout::BOARD_X + layout::BOARD_SIZE / 2.f, layout::BOARD_Y + layout::BOARD_SIZE + 14.f}, 15,
             palette::TextFaint, Weight::Regular, Align::Center);

    // Right: the options card.
    drawRoundedRect(target, {layout::PANEL_X, layout::PANEL_Y, layout::PANEL_W, layout::PANEL_H}, 14.f, palette::Panel);
    const float x = layout::PANEL_X + 24.f;
    const float right = layout::PANEL_X + layout::PANEL_W - 24.f;
    drawText(target, f, "New game", {x, 34.f}, 30, palette::Text, Weight::Bold);
    drawText(target, f, "Choose your opponent and time control", {x, 76.f}, 15, palette::TextDim);

    drawText(target, f, "Opponent", {x, 112.f}, 15, palette::TextDim, Weight::SemiBold);
    drawText(target, f, "You play as", {x, 196.f}, 15, config_.vsComputer ? palette::TextDim : palette::TextFaint,
             Weight::SemiBold);
    drawText(target, f, "Computer level", {x, 280.f}, 15, config_.vsComputer ? palette::TextDim : palette::TextFaint,
             Weight::SemiBold);
    if (config_.vsComputer)
        drawText(target, f, aiLevelName(config_.aiLevel), {right, 280.f}, 15, palette::Accent, Weight::SemiBold, Align::Right);
    drawText(target, f, "Time control", {x, 364.f}, 15, palette::TextDim, Weight::SemiBold);
    drawText(target, f, config_.timeControl.enabled() ? "minutes + seconds per move" : "untimed game", {right, 364.f}, 15,
             palette::TextFaint, Weight::Regular, Align::Right);

    for (const Button& b : buttons_) b.draw(target, f, mouse);

    drawText(target, f, "In game:  U undo  \xC2\xB7  F flip  \xC2\xB7  H hint  \xC2\xB7  Left/Right review  \xC2\xB7  M mute",
             {layout::PANEL_X + layout::PANEL_W / 2.f, 712.f}, 14, palette::TextFaint, Weight::Regular, Align::Center);
    drawText(target, f, "Ctrl+S save PGN  \xC2\xB7  Ctrl+C copy PGN  \xC2\xB7  Ctrl+V paste FEN/PGN",
             {layout::PANEL_X + layout::PANEL_W / 2.f, 734.f}, 14, palette::TextFaint, Weight::Regular, Align::Center);

    if (dialog_ != Dialog::None) modal_.draw(target, f, mouse);
}

}  // namespace gui
