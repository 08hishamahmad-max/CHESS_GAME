// MenuScreen.h - the start screen: choose opponent, colour, computer level
// and time control, then start (or load a game / position).
#pragma once

#include "AppContext.h"
#include "Dialogs.h"
#include "engine/Board.h"

#include <SFML/Graphics.hpp>

#include <string>
#include <vector>

namespace gui {

class MenuScreen {
public:
    explicit MenuScreen(AppContext& ctx);

    void handleEvent(const sf::Event& event, sf::Vector2f mouse);
    void draw(sf::RenderTarget& target, sf::Vector2f mouse);
    bool wantsHandCursor(sf::Vector2f mouse) const;

    // Set when the player pressed Start / loaded a game / pressed Quit.
    ScreenRequest takeRequest();
    const GameConfig& config() const { return config_; }

private:
    enum Id {
        OppFriend = 1, OppComputer, ColorWhite, ColorRandom, ColorBlack,
        Level1, Level2, Level3, Level4, Level5,
        Time0, Time1, Time2, Time3, Time4, Time5, TimeCustom,
        Start, LoadPgn, PastePosition, OpenSettings, Quit,
        // dialog buttons
        DlgOk = 100, DlgCancel, DlgFileBase = 200
    };
    enum class Dialog { None, CustomTime, Position, Load, Settings };

    void rebuild();
    void onButton(int id);
    void onDialogButton(int id);
    void openLoadDialog();
    void openSettingsDialog();
    void startWith(const std::string& fen, const std::string& pgn);

    AppContext& ctx_;
    std::vector<Button> buttons_;
    Modal modal_;
    Dialog dialog_ = Dialog::None;
    std::vector<std::string> files_;  // for the load dialog
    GameConfig config_;
    ScreenRequest request_ = ScreenRequest::None;
    chess::Board decorBoard_;
};

}  // namespace gui
