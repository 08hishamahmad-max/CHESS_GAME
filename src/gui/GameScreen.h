// GameScreen.h - the playing screen: board, player cards with clocks and
// captured pieces, move list with review mode, action buttons and dialogs.
// It connects the user's input with the engine (Game, ChessClock, AsyncSearch).
#pragma once

#include "AppContext.h"
#include "Dialogs.h"
#include "Renderer.h"
#include "engine/AsyncSearch.h"
#include "engine/ChessClock.h"
#include "engine/Game.h"

#include <SFML/Graphics.hpp>

#include <array>
#include <string>
#include <vector>

namespace gui {

class GameScreen {
public:
    explicit GameScreen(AppContext& ctx);

    // Starts a new game with these options (also used for "rematch").
    void start(const GameConfig& config);

    void handleEvent(const sf::Event& event, sf::Vector2f mouse);
    void update(float dt);
    void draw(sf::RenderTarget& target, sf::Vector2f mouse);
    bool wantsHandCursor(sf::Vector2f mouse) const;

    ScreenRequest takeRequest();

private:
    // ---- button / dialog ids ----
    enum Id {
        BtnFirst = 1, BtnPrev, BtnNext, BtnLast, BtnFlip,
        BtnHint, BtnUndo, BtnNewGame,
        BtnOfferDraw, BtnClaimDraw, BtnResign,
        BtnSave, BtnLoad, BtnCopyPgn,
        BtnCopyFen, BtnPaste, BtnSettings,
        BtnMenu, BtnSound, BtnTheme,
        DlgOk = 100, DlgCancel, DlgRematch, DlgReview, DlgMenu, DlgAccept, DlgDecline, DlgFileBase = 200
    };
    enum class Dialog { None, GameOver, ConfirmResign, DrawOffer, Position, Load, Settings };
    enum class AiTask { None, Move, Hint };

    // ---- game flow ----
    bool isComputerTurn() const;
    bool humanCanMove() const;
    void applyMove(const chess::Move& move, bool animate);
    void undo();
    void startComputerIfNeeded();
    void onGameEnded();
    void loadPosition(const std::string& fen, const std::string& pgnText);

    // ---- input ----
    void onBoardPress(sf::Vector2f pos);
    void onBoardRelease(sf::Vector2f pos);
    bool tryMove(chess::Square from, chess::Square to, bool animate);
    void select(chess::Square sq);
    void clearSelection();
    void onButton(int id);
    void onDialogButton(int id);
    void onKey(const sf::Event::KeyEvent& key);

    // ---- review mode ----
    bool reviewing() const { return viewPly_ >= 0; }
    void setViewPly(int ply, bool animate);
    const chess::Board& displayedBoard() const;

    // ---- dialogs ----
    void openDialog(Dialog kind, Modal modal);
    void closeDialog();
    void showToast(const std::string& text);
    void openPositionDialog();
    void openLoadDialog();

    // ---- drawing ----
    void rebuildButtons();
    void drawPlayerCard(sf::RenderTarget& target, chess::Color color, bool top);
    void drawSidePanel(sf::RenderTarget& target, sf::Vector2f mouse);
    void drawMoveList(sf::RenderTarget& target, sf::FloatRect area, sf::Vector2f mouse);
    std::string playerName(chess::Color c) const;
    std::string statusLine() const;
    std::string pgnText() const;
    void startAnimation(const chess::Board& before, const chess::Move& m);
    sf::FloatRect moveCellRect(int ply) const;

    AppContext& ctx_;
    GameConfig config_;
    chess::Game game_;
    chess::ChessClock clock_;
    std::vector<std::array<long long, 2>> clockHistory_;  // clock after each ply (for undo)
    double clockAccumulatorMs_ = 0.0;
    chess::AsyncSearch ai_;
    AiTask aiTask_ = AiTask::None;
    int lastAiScore_ = 0;     // last evaluation from the computer's point of view
    bool haveAiScore_ = false;
    std::string aiInfo_;      // e.g. "depth 7 · +0.35"

    // board interaction
    bool flipped_ = false;
    chess::Square selected_ = chess::NO_SQUARE;
    std::vector<chess::Square> targets_;
    bool dragging_ = false;
    bool pressedOnSelected_ = false;  // second click on the selected piece deselects it
    sf::Vector2f dragPos_;
    chess::Square promoFrom_ = chess::NO_SQUARE, promoTo_ = chess::NO_SQUARE;
    bool promoAnimate_ = false;
    MoveAnimation animation_;
    float animationTime_ = 0.f;
    chess::Square hintFrom_ = chess::NO_SQUARE, hintTo_ = chess::NO_SQUARE;

    // review mode
    int viewPly_ = -1;  // -1 = live position, otherwise number of plies shown
    chess::Board reviewBoard_;

    // panel
    std::vector<Button> buttons_;
    int moveListScroll_ = 0;  // first visible row
    bool followLatest_ = true;
    std::string openingName_;
    std::string toast_;
    float toastTime_ = 0.f;

    // dialogs
    Modal modal_;
    Dialog dialog_ = Dialog::None;
    std::vector<std::string> files_;
    bool gameOverShown_ = false;
    float gameOverTimer_ = -1.f;  // delay before the game-over dialog appears

    ScreenRequest request_ = ScreenRequest::None;
};

}  // namespace gui
