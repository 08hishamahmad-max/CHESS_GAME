// GameScreen.cpp
#include "GameScreen.h"

#include "engine/AiLevel.h"
#include "engine/MoveGenerator.h"
#include "engine/Notation.h"
#include "engine/OpeningBook.h"
#include "engine/Pgn.h"

#include <SFML/Window/Clipboard.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <random>

using namespace chess;

namespace gui {

namespace {

// Layout of the screen (see also Ui.h).
constexpr float CARD_H = 40.f;
constexpr float TOP_CARD_Y = layout::BOARD_Y - CARD_H - 8.f;
constexpr float BOTTOM_CARD_Y = layout::BOARD_Y + layout::BOARD_SIZE + 8.f;
constexpr float CLOCK_W = 132.f;
constexpr float HEADER_Y = layout::PANEL_Y + 14.f;
constexpr float LIST_Y = 112.f;
constexpr float LIST_H = 356.f;
constexpr float ROW_H = 30.f;
constexpr float NAV_Y = LIST_Y + LIST_H + 12.f;
constexpr float GRID_Y = NAV_Y + 54.f;
constexpr float ANIMATION_SECONDS = 0.18f;

const int MATERIAL[7] = {0, 1, 3, 3, 5, 9, 0};  // classic point values for the +N display

// "2024-05-01_14-30-05" or "2024.05.01" depending on the format.
std::string timestamp(const char* format) {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof buf, format, &tm);
    return buf;
}

std::string capitalise(std::string s) {
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = static_cast<char>(s[0] - 'a' + 'A');
    return s;
}

std::string formatEval(int cp) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%+.2f", cp / 100.0);
    return buf;
}

}  // namespace

GameScreen::GameScreen(AppContext& ctx) : ctx_(ctx) {}

ScreenRequest GameScreen::takeRequest() {
    ScreenRequest r = request_;
    request_ = ScreenRequest::None;
    return r;
}

// ===========================================================================
// Game flow
// ===========================================================================
void GameScreen::start(const GameConfig& config) {
    ai_.cancel();
    aiTask_ = AiTask::None;
    config_ = config;
    if (config_.vsComputer && config_.randomColor) {
        static std::mt19937 rng{std::random_device{}()};
        config_.humanColor = (rng() & 1) ? Color::White : Color::Black;
    }

    std::string error;
    if (!config_.pgn.empty()) {
        if (!pgn::read(config_.pgn, game_, nullptr, error)) {
            game_.newGame();
            showToast("Could not load PGN: " + error);
        }
    } else if (!config_.startFen.empty()) {
        if (!game_.newGame(config_.startFen)) {
            game_.newGame();
            showToast("Invalid FEN - starting from the normal position");
        }
    } else {
        game_.newGame();
    }

    // Loaded games are continued without a clock.
    clock_.reset(config_.pgn.empty() ? config_.timeControl : TimeControl{});
    clockHistory_.assign(static_cast<std::size_t>(game_.plyCount()) + 1,
                         {clock_.remainingMs(Color::White), clock_.remainingMs(Color::Black)});
    clockAccumulatorMs_ = 0.0;

    flipped_ = config_.vsComputer && config_.humanColor == Color::Black;
    clearSelection();
    promoFrom_ = promoTo_ = NO_SQUARE;
    animation_.active = false;
    hintFrom_ = hintTo_ = NO_SQUARE;
    viewPly_ = -1;
    followLatest_ = true;
    haveAiScore_ = false;
    aiInfo_.clear();
    openingName_.clear();
    if (game_.startFen() == Board().toFen()) openingName_ = OpeningBook::instance().openingName(game_.moves());
    closeDialog();
    gameOverShown_ = game_.isOver();  // a finished game loaded from PGN: no pop-up
    gameOverTimer_ = -1.f;
    ctx_.sounds.play(SoundEffect::Notify);
}

bool GameScreen::isComputerTurn() const {
    return config_.vsComputer && !game_.isOver() && game_.sideToMove() != config_.humanColor;
}

bool GameScreen::humanCanMove() const {
    return !game_.isOver() && !isComputerTurn() && dialog_ == Dialog::None && !reviewing();
}

void GameScreen::applyMove(const Move& move, bool animate) {
    if (aiTask_ == AiTask::Hint) {  // a hint is useless once a move is made
        ai_.cancel();
        aiTask_ = AiTask::None;
    }
    const Board before = game_.board();
    const Color mover = before.sideToMove();
    const bool capture = !before.at(move.to).isEmpty() || move.flag == MoveFlag::EnPassant;
    if (!game_.makeMove(move)) return;

    if (clock_.enabled()) clock_.moveMade(mover);
    clockHistory_.push_back({clock_.remainingMs(Color::White), clock_.remainingMs(Color::Black)});

    animation_.active = false;
    if (animate && ctx_.settings.animations) startAnimation(before, game_.lastMove());

    if (!game_.isOver()) {
        if (game_.board().inCheck()) ctx_.sounds.play(SoundEffect::Check);
        else if (game_.lastMove().isCastle()) ctx_.sounds.play(SoundEffect::Castle);
        else if (capture) ctx_.sounds.play(SoundEffect::Capture);
        else ctx_.sounds.play(SoundEffect::Move);
    }

    clearSelection();
    promoFrom_ = promoTo_ = NO_SQUARE;
    hintFrom_ = hintTo_ = NO_SQUARE;
    viewPly_ = -1;
    followLatest_ = true;
    if (game_.startFen() == Board().toFen()) {
        std::string name = OpeningBook::instance().openingName(game_.moves());
        if (!name.empty()) openingName_ = name;
    }
    if (!config_.vsComputer && ctx_.settings.autoFlip) flipped_ = game_.sideToMove() == Color::Black;
    if (game_.isOver()) onGameEnded();
}

void GameScreen::startAnimation(const Board& before, const Move& m) {
    animation_ = MoveAnimation{};
    animation_.active = true;
    animation_.piece = before.at(m.from);
    if (m.isPromotion()) animation_.piece = Piece(m.promotion, before.sideToMove());
    animation_.from = m.from;
    animation_.to = m.to;
    if (m.isCastle()) {
        bool kingside = m.flag == MoveFlag::CastleKingside;
        animation_.rookFrom = kingside ? m.to + 1 : m.to - 2;
        animation_.rookTo = kingside ? m.to - 1 : m.to + 1;
        animation_.rook = before.at(animation_.rookFrom);
    }
    animation_.progress = 0.f;
    animationTime_ = 0.f;
}

void GameScreen::undo() {
    if (game_.plyCount() == 0) return;
    ai_.cancel();
    aiTask_ = AiTask::None;
    promoFrom_ = promoTo_ = NO_SQUARE;
    if (dialog_ == Dialog::GameOver) closeDialog();
    // Against the computer take back its reply too, so it is the player's turn.
    do {
        game_.undo();
        if (clockHistory_.size() > 1) clockHistory_.pop_back();
    } while (config_.vsComputer && game_.plyCount() > 0 && game_.sideToMove() != config_.humanColor);
    const auto& c = clockHistory_.back();
    clock_.setRemainingMs(Color::White, c[0]);
    clock_.setRemainingMs(Color::Black, c[1]);

    gameOverShown_ = false;
    gameOverTimer_ = -1.f;
    animation_.active = false;
    hintFrom_ = hintTo_ = NO_SQUARE;
    viewPly_ = -1;
    followLatest_ = true;
    clearSelection();
    if (!config_.vsComputer && ctx_.settings.autoFlip) flipped_ = game_.sideToMove() == Color::Black;
    ctx_.sounds.play(SoundEffect::Move);
}

void GameScreen::startComputerIfNeeded() {
    if (aiTask_ != AiTask::None || !isComputerTurn()) return;
    const bool timed = clock_.enabled();
    SearchLimits limits =
        aiLimitsForLevel(config_.aiLevel, timed ? clock_.remainingMs(game_.sideToMove()) : 0,
                         timed ? clock_.timeControl().incrementSeconds * 1000 : 0);
    ai_.start(game_.board(), limits, game_.recentPositionKeys());
    aiTask_ = AiTask::Move;
}

void GameScreen::onGameEnded() {
    ai_.cancel();
    aiTask_ = AiTask::None;
    clearSelection();
    promoFrom_ = promoTo_ = NO_SQUARE;
    ctx_.sounds.play(SoundEffect::GameEnd);
    gameOverTimer_ = 0.8f;  // show the dialog after the last move is visible
}

void GameScreen::loadPosition(const std::string& fen, const std::string& pgnText) {
    GameConfig cfg = config_;
    cfg.startFen = fen;
    cfg.pgn = pgnText;
    cfg.randomColor = false;
    // When loading a position against the computer, the player takes the side to move.
    if (cfg.vsComputer && !fen.empty()) cfg.humanColor = Board(fen).sideToMove();
    start(cfg);
    showToast(pgnText.empty() ? "Position loaded" : "Game loaded");
}

// ===========================================================================
// Update (called every frame)
// ===========================================================================
void GameScreen::update(float dt) {
    if (toastTime_ > 0.f) toastTime_ -= dt;

    if (animation_.active) {
        animationTime_ += dt;
        animation_.progress = std::min(1.f, animationTime_ / ANIMATION_SECONDS);
        if (animation_.progress >= 1.f) animation_.active = false;
    }

    // Clocks run once both sides have made their first move.
    if (clock_.enabled() && !game_.isOver() && game_.plyCount() >= 2) {
        clockAccumulatorMs_ += static_cast<double>(dt) * 1000.0;
        long long whole = static_cast<long long>(clockAccumulatorMs_);
        clockAccumulatorMs_ -= static_cast<double>(whole);
        const Color side = game_.sideToMove();
        clock_.tick(side, whole);
        if (clock_.flagged(side)) {
            game_.flagFall(side);
            onGameEnded();
        }
    }

    // Computer: collect a finished search (after the previous animation ended).
    if (aiTask_ != AiTask::None && ai_.isFinished() && !animation_.active) {
        SearchResult r = ai_.takeResult();
        AiTask task = aiTask_;
        aiTask_ = AiTask::None;
        if (task == AiTask::Move && isComputerTurn() && !r.best.isNull()) {
            if (r.fromBook) {
                aiInfo_ = "Book move";
            } else {
                lastAiScore_ = r.score;
                haveAiScore_ = true;
                aiInfo_ = "Depth " + std::to_string(r.depth) + "  \xC2\xB7  " +
                          (r.isMateScore() ? "Mate in " + std::to_string(std::abs(r.mateInMoves()))
                                           : formatEval(game_.sideToMove() == Color::White ? r.score : -r.score));
            }
            // Claim a draw by repetition / 50 moves if the computer is not better.
            if (game_.claimableDraw() != EndReason::None && haveAiScore_ && lastAiScore_ <= 0) {
                game_.claimDraw();
                onGameEnded();
            } else {
                applyMove(r.best, true);
            }
        } else if (task == AiTask::Hint && humanCanMove() && !r.best.isNull()) {
            hintFrom_ = r.best.from;
            hintTo_ = r.best.to;
            Board b = game_.board();
            showToast("Hint: " + notation::toSan(b, r.best));
        }
    }
    startComputerIfNeeded();

    if (gameOverTimer_ > 0.f) {
        gameOverTimer_ -= dt;
        if (gameOverTimer_ <= 0.f && game_.isOver() && !gameOverShown_) {
            gameOverShown_ = true;
            const GameResult& res = game_.result();
            std::string title = res.headline();
            if (config_.vsComputer && res.outcome != Outcome::Draw) {
                bool humanWon = (res.outcome == Outcome::WhiteWins) == (config_.humanColor == Color::White);
                title = humanWon ? "You won!" : "Computer wins";
            }
            Modal m(title, {capitalise(res.headline() == "Draw" ? "Draw " + res.reasonText()
                                                                : res.headline() + " " + res.reasonText()),
                            std::to_string((game_.plyCount() + 1) / 2) + ((game_.plyCount() + 1) / 2 == 1 ? " move" : " moves") +
                                "  \xC2\xB7  " + res.pgnString()});
            m.addButton(DlgRematch, "Rematch", true);
            m.addButton(DlgReview, "Review");
            m.addButton(DlgMenu, "Menu");
            m.defaultId = DlgRematch;
            m.cancelId = DlgReview;
            m.layout(false, 470.f);
            openDialog(Dialog::GameOver, m);
        }
    }
}

// ===========================================================================
// Input
// ===========================================================================
void GameScreen::handleEvent(const sf::Event& event, sf::Vector2f mouse) {
    rebuildButtons();
    if (dialog_ != Dialog::None) {
        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left)
            onDialogButton(modal_.onClick(mouse));
        else if (event.type == sf::Event::KeyPressed)
            onDialogButton(modal_.onKey(event.key));
        else if (event.type == sf::Event::TextEntered)
            modal_.onText(event.text.unicode);
        return;
    }

    switch (event.type) {
        case sf::Event::MouseButtonPressed:
            if (event.mouseButton.button == sf::Mouse::Left) {
                int id = hitTest(buttons_, mouse);
                if (id >= 0 && promoTo_ == NO_SQUARE) {
                    onButton(id);
                    return;
                }
                // Click in the move list: jump to that position.
                sf::FloatRect list(layout::PANEL_X, LIST_Y, layout::PANEL_W, LIST_H);
                if (list.contains(mouse) && promoTo_ == NO_SQUARE) {
                    for (int p = 0; p < game_.plyCount(); ++p)
                        if (moveCellRect(p).contains(mouse)) setViewPly(p + 1, false);
                    return;
                }
                onBoardPress(mouse);
            } else if (event.mouseButton.button == sf::Mouse::Right) {
                clearSelection();
                hintFrom_ = hintTo_ = NO_SQUARE;
            }
            break;
        case sf::Event::MouseButtonReleased:
            if (event.mouseButton.button == sf::Mouse::Left) onBoardRelease(mouse);
            break;
        case sf::Event::MouseMoved:
            if (dragging_) dragPos_ = mouse;
            break;
        case sf::Event::MouseWheelScrolled: {
            sf::FloatRect list(layout::PANEL_X, LIST_Y, layout::PANEL_W, LIST_H);
            if (list.contains(mouse)) {
                moveListScroll_ = std::max(0, moveListScroll_ - static_cast<int>(event.mouseWheelScroll.delta));
                followLatest_ = false;
            } else if (Renderer::squareAt(mouse, flipped_) != NO_SQUARE) {
                // Scrolling over the board steps through the game.
                int shown = reviewing() ? viewPly_ : game_.plyCount();
                setViewPly(shown + (event.mouseWheelScroll.delta < 0 ? 1 : -1), false);
            }
            break;
        }
        case sf::Event::KeyPressed:
            onKey(event.key);
            break;
        default:
            break;
    }
}

void GameScreen::onKey(const sf::Event::KeyEvent& key) {
    const bool ctrl = key.control || key.system;
    const int shown = reviewing() ? viewPly_ : game_.plyCount();
    switch (key.code) {
        case sf::Keyboard::Left:  setViewPly(shown - 1, false); break;
        case sf::Keyboard::Right: setViewPly(shown + 1, true); break;
        case sf::Keyboard::Up:
        case sf::Keyboard::Home:  setViewPly(0, false); break;
        case sf::Keyboard::Down:
        case sf::Keyboard::End:   setViewPly(game_.plyCount(), false); break;
        case sf::Keyboard::U:     onButton(BtnUndo); break;
        case sf::Keyboard::Z:     if (ctrl) onButton(BtnUndo); break;
        case sf::Keyboard::F:     onButton(BtnFlip); break;
        case sf::Keyboard::H:     onButton(BtnHint); break;
        case sf::Keyboard::M:     onButton(BtnSound); break;
        case sf::Keyboard::N:     onButton(BtnNewGame); break;
        case sf::Keyboard::S:     if (ctrl) onButton(BtnSave); break;
        case sf::Keyboard::C:     if (ctrl) onButton(BtnCopyPgn); break;
        case sf::Keyboard::V:     if (ctrl) onButton(BtnPaste); break;
        case sf::Keyboard::Escape:
            if (promoTo_ != NO_SQUARE) promoFrom_ = promoTo_ = NO_SQUARE;
            clearSelection();
            hintFrom_ = hintTo_ = NO_SQUARE;
            break;
        default: break;
    }
}

void GameScreen::onBoardPress(sf::Vector2f pos) {
    // The promotion picker is modal.
    if (promoTo_ != NO_SQUARE) {
        PieceType choice = Renderer::promotionChoiceAt(pos, promoTo_, flipped_);
        Square from = promoFrom_, to = promoTo_;
        promoFrom_ = promoTo_ = NO_SQUARE;
        if (choice != PieceType::None) applyMove(Move(from, to, MoveFlag::Normal, choice), promoAnimate_);
        else clearSelection();
        return;
    }
    Square sq = Renderer::squareAt(pos, flipped_);
    if (sq == NO_SQUARE) {
        clearSelection();
        return;
    }
    if (reviewing()) {  // clicking the board returns to the live game
        setViewPly(game_.plyCount(), false);
        return;
    }
    if (!humanCanMove()) return;

    if (selected_ != NO_SQUARE && sq != selected_ &&
        std::find(targets_.begin(), targets_.end(), sq) != targets_.end()) {
        tryMove(selected_, sq, true);
        return;
    }
    const Piece& p = game_.board().at(sq);
    if (!p.isEmpty() && p.color == game_.sideToMove()) {
        pressedOnSelected_ = (sq == selected_);
        if (!pressedOnSelected_) select(sq);
        dragging_ = true;
        dragPos_ = pos;
    } else {
        clearSelection();
    }
}

void GameScreen::onBoardRelease(sf::Vector2f pos) {
    if (!dragging_) return;
    dragging_ = false;
    Square sq = Renderer::squareAt(pos, flipped_);
    if (sq == selected_) {
        if (pressedOnSelected_) clearSelection();  // clicked the selected piece again
        return;
    }
    if (sq != NO_SQUARE && std::find(targets_.begin(), targets_.end(), sq) != targets_.end())
        tryMove(selected_, sq, false);  // dropped: no animation needed
    // otherwise the piece snaps back and stays selected
}

bool GameScreen::tryMove(Square from, Square to, bool animate) {
    std::vector<Move> candidates;
    for (const Move& m : game_.legalMovesFrom(from))
        if (m.to == to) candidates.push_back(m);
    if (candidates.empty()) return false;
    if (candidates.front().isPromotion()) {
        if (ctx_.settings.autoQueen) {
            applyMove(Move(from, to, MoveFlag::Normal, PieceType::Queen), animate);
        } else {
            promoFrom_ = from;
            promoTo_ = to;
            promoAnimate_ = animate;
            dragging_ = false;
        }
        return true;
    }
    applyMove(candidates.front(), animate);
    return true;
}

void GameScreen::select(Square sq) {
    selected_ = sq;
    targets_.clear();
    for (const Move& m : game_.legalMovesFrom(sq))
        if (std::find(targets_.begin(), targets_.end(), m.to) == targets_.end()) targets_.push_back(m.to);
}

void GameScreen::clearSelection() {
    selected_ = NO_SQUARE;
    targets_.clear();
    dragging_ = false;
    pressedOnSelected_ = false;
}

// ===========================================================================
// Review mode
// ===========================================================================
void GameScreen::setViewPly(int ply, bool animate) {
    const int total = game_.plyCount();
    ply = std::max(0, std::min(ply, total));
    const int shownBefore = reviewing() ? viewPly_ : total;
    if (ply == shownBefore) return;
    clearSelection();
    promoFrom_ = promoTo_ = NO_SQUARE;
    if (animate && ply == shownBefore + 1 && ctx_.settings.animations)
        startAnimation(game_.boardAtPly(ply - 1), game_.moves()[static_cast<std::size_t>(ply - 1)]);
    else
        animation_.active = false;
    if (ply >= total) {
        viewPly_ = -1;
        followLatest_ = true;
    } else {
        viewPly_ = ply;
        reviewBoard_ = game_.boardAtPly(ply);
        followLatest_ = false;
        // keep the highlighted move visible
        int row = 0;
        if (ply > 0) {
            bool startBlack = Board(game_.startFen()).sideToMove() == Color::Black;
            row = (ply - 1 + (startBlack ? 1 : 0)) / 2;
        }
        const int visible = static_cast<int>((LIST_H - 16.f) / ROW_H);
        if (row < moveListScroll_) moveListScroll_ = row;
        if (row >= moveListScroll_ + visible) moveListScroll_ = row - visible + 1;
    }
    ctx_.sounds.play(SoundEffect::Move);
}

const Board& GameScreen::displayedBoard() const { return reviewing() ? reviewBoard_ : game_.board(); }

// ===========================================================================
// Buttons and dialogs
// ===========================================================================
void GameScreen::onButton(int id) {
    switch (id) {
        case BtnFirst: setViewPly(0, false); break;
        case BtnPrev:  setViewPly((reviewing() ? viewPly_ : game_.plyCount()) - 1, false); break;
        case BtnNext:  setViewPly((reviewing() ? viewPly_ : game_.plyCount()) + 1, true); break;
        case BtnLast:  setViewPly(game_.plyCount(), false); break;
        case BtnFlip:  flipped_ = !flipped_; break;
        case BtnUndo:  undo(); break;
        case BtnHint:
            if (humanCanMove() && aiTask_ == AiTask::None) {
                SearchLimits l = aiLimitsForLevel(4);
                l.timeMs = 1000;
                ai_.start(game_.board(), l, game_.recentPositionKeys());
                aiTask_ = AiTask::Hint;
                showToast("Thinking about a hint...");
            }
            break;
        case BtnNewGame: {
            GameConfig cfg = config_;
            cfg.pgn.clear();
            if (!cfg.startFen.empty()) cfg.startFen = game_.startFen();
            start(cfg);
            break;
        }
        case BtnOfferDraw:
            if (game_.isOver()) break;
            if (config_.vsComputer) {
                if (haveAiScore_ && lastAiScore_ <= -150) {
                    game_.agreeDraw();
                    onGameEnded();
                } else {
                    showToast("The computer declines your draw offer");
                }
            } else {
                std::string offering = game_.sideToMove() == Color::White ? "White" : "Black";
                std::string other = game_.sideToMove() == Color::White ? "Black" : "White";
                Modal m("Draw offer", {offering + " offers a draw.", other + ", do you accept?"});
                m.addButton(DlgDecline, "Decline");
                m.addButton(DlgAccept, "Accept", true);
                m.cancelId = DlgDecline;
                m.layout();
                openDialog(Dialog::DrawOffer, m);
            }
            break;
        case BtnClaimDraw:
            if (game_.claimDraw()) onGameEnded();
            break;
        case BtnResign:
            if (!game_.isOver()) {
                Modal m("Resign?", {"Do you really want to give up this game?"});
                m.addButton(DlgCancel, "Cancel");
                m.addButton(DlgOk, "Resign", true);
                m.cancelId = DlgCancel;
                m.defaultId = DlgOk;
                m.accent = palette::Danger;
                m.layout();
                openDialog(Dialog::ConfirmResign, m);
            }
            break;
        case BtnSave: {
            std::string path = ctx_.savesDir + "/chess_" + timestamp("%Y-%m-%d_%H-%M-%S") + ".pgn";
            showToast(writeTextFile(path, pgnText()) ? "Saved " + path : "Could not write " + path);
            break;
        }
        case BtnLoad:     openLoadDialog(); break;
        case BtnCopyPgn:
            sf::Clipboard::setString(pgnText());
            showToast("PGN copied to the clipboard");
            break;
        case BtnCopyFen:
            sf::Clipboard::setString(displayedBoard().toFen());
            showToast("FEN copied to the clipboard");
            break;
        case BtnPaste:    openPositionDialog(); break;
        case BtnSettings: openDialog(Dialog::Settings, settings_dialog::make(ctx_.settings)); break;
        case BtnMenu:
            ai_.cancel();
            aiTask_ = AiTask::None;
            request_ = ScreenRequest::OpenMenu;
            break;
        case BtnSound:
            ctx_.settings.sound = !ctx_.settings.sound;
            ctx_.sounds.setEnabled(ctx_.settings.sound);
            ctx_.saveSettings();
            showToast(ctx_.settings.sound ? "Sound on" : "Sound off");
            break;
        case BtnTheme:
            ctx_.settings.boardTheme = (ctx_.settings.boardTheme + 1) % static_cast<int>(boardThemes().size());
            ctx_.saveSettings();
            break;
        default: break;
    }
}

void GameScreen::openDialog(Dialog kind, Modal modal) {
    modal_ = std::move(modal);
    dialog_ = kind;
    clearSelection();
}

void GameScreen::closeDialog() {
    modal_.close();
    dialog_ = Dialog::None;
}

void GameScreen::openPositionDialog() {
    Modal m("Load a position or game", {"Paste a FEN string or a PGN game (Ctrl+V)"});
    std::string clip = clipboardText();
    for (char& c : clip)
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    m.enableTextField(clip.substr(0, 400), "FEN or PGN");
    m.addButton(DlgCancel, "Cancel");
    m.addButton(DlgOk, "Load", true);
    m.defaultId = DlgOk;
    m.cancelId = DlgCancel;
    m.layout(false, 640.f);
    openDialog(Dialog::Position, m);
}

void GameScreen::openLoadDialog() {
    files_ = listPgnFiles(ctx_.savesDir);
    Modal m("Load a saved game",
            files_.empty() ? std::vector<std::string>{"No saved games in the \"" + ctx_.savesDir + "\" folder yet."}
                           : std::vector<std::string>{"Games saved with \"Save PGN\" (newest first)"});
    for (std::size_t i = 0; i < files_.size(); ++i) m.addButton(DlgFileBase + static_cast<int>(i), files_[i]);
    m.addButton(DlgCancel, "Cancel");
    m.cancelId = DlgCancel;
    m.layout(true, 520.f);
    openDialog(Dialog::Load, m);
}

void GameScreen::onDialogButton(int id) {
    if (id < 0) return;
    switch (dialog_) {
        case Dialog::GameOver:
            if (id == DlgRematch) {
                closeDialog();
                onButton(BtnNewGame);
                return;
            }
            if (id == DlgMenu) {
                closeDialog();
                onButton(BtnMenu);
                return;
            }
            break;  // Review: just close
        case Dialog::ConfirmResign:
            if (id == DlgOk) {
                game_.resign(config_.vsComputer ? config_.humanColor : game_.sideToMove());
                closeDialog();
                onGameEnded();
                return;
            }
            break;
        case Dialog::DrawOffer:
            if (id == DlgAccept) {
                game_.agreeDraw();
                closeDialog();
                onGameEnded();
                return;
            }
            showToast("Draw offer declined");
            break;
        case Dialog::Position:
            if (id == DlgOk) {
                const std::string text = modal_.text();
                Board test;
                Game g;
                std::string err;
                if (test.loadFen(text)) {
                    closeDialog();
                    loadPosition(text, "");
                } else if (pgn::read(text, g, nullptr, err)) {
                    closeDialog();
                    loadPosition("", text);
                } else {
                    modal_.setError("Not a valid FEN or PGN" + (err.empty() ? "" : ": " + err));
                }
                return;
            }
            break;
        case Dialog::Load:
            if (id >= DlgFileBase) {
                std::size_t index = static_cast<std::size_t>(id - DlgFileBase);
                std::string text, err;
                Game g;
                if (index < files_.size() && readTextFile(ctx_.savesDir + "/" + files_[index], text) &&
                    pgn::read(text, g, nullptr, err)) {
                    closeDialog();
                    loadPosition("", text);
                } else {
                    modal_.setLines({"Could not read that file:", err.empty() ? "file not readable" : err});
                }
                return;
            }
            break;
        case Dialog::Settings:
            if (settings_dialog::apply(ctx_.settings, id)) {
                ctx_.sounds.setEnabled(ctx_.settings.sound);
                ctx_.saveSettings();
                modal_ = settings_dialog::make(ctx_.settings);
                return;
            }
            break;
        default:
            break;
    }
    closeDialog();
}

void GameScreen::showToast(const std::string& text) {
    toast_ = text;
    toastTime_ = 2.6f;
}

std::string GameScreen::playerName(Color c) const {
    if (!config_.vsComputer) return c == Color::White ? "White" : "Black";
    if (c == config_.humanColor) return ctx_.settings.playerName;
    return "Computer";
}

std::string GameScreen::pgnText() const {
    PgnTags tags = {{"Event", "Casual game"},
                    {"Site", "ChessGame (C++ / SFML)"},
                    {"Date", timestamp("%Y.%m.%d")},
                    {"Round", "-"},
                    {"White", playerName(Color::White)},
                    {"Black", playerName(Color::Black)}};
    if (config_.vsComputer) tags.emplace_back("Annotator", "Computer level: " + aiLevelName(config_.aiLevel));
    if (clock_.enabled())
        tags.emplace_back("TimeControl", std::to_string(clock_.timeControl().baseSeconds) + "+" +
                                             std::to_string(clock_.timeControl().incrementSeconds));
    if (!openingName_.empty()) tags.emplace_back("Opening", openingName_);
    if (game_.isOver()) tags.emplace_back("Termination", capitalise(game_.result().reasonText()));
    return pgn::write(game_, tags);
}

bool GameScreen::wantsHandCursor(sf::Vector2f mouse) const {
    if (dialog_ != Dialog::None) return hitTest(modal_.buttons(), mouse) >= 0;
    if (hitTest(buttons_, mouse) >= 0) return true;
    if (promoTo_ != NO_SQUARE) return Renderer::promotionChoiceAt(mouse, promoTo_, flipped_) != PieceType::None;
    for (int p = 0; p < game_.plyCount(); ++p)
        if (moveCellRect(p).contains(mouse) && sf::FloatRect(layout::PANEL_X, LIST_Y, layout::PANEL_W, LIST_H).contains(mouse))
            return true;
    Square sq = Renderer::squareAt(mouse, flipped_);
    if (sq == NO_SQUARE || !humanCanMove()) return false;
    const Piece& p = game_.board().at(sq);
    return (!p.isEmpty() && p.color == game_.sideToMove()) ||
           std::find(targets_.begin(), targets_.end(), sq) != targets_.end();
}

// ===========================================================================
// Drawing
// ===========================================================================
void GameScreen::rebuildButtons() {
    buttons_.clear();
    const float x = layout::PANEL_X + 14.f, w = layout::PANEL_W - 28.f, gap = 8.f;
    auto add = [&](int id, sf::FloatRect r, const std::string& label, bool enabled = true, Icon icon = Icon::None) {
        Button b;
        b.id = id;
        b.rect = r;
        b.label = label;
        b.enabled = enabled;
        b.icon = icon;
        b.fontSize = 14;
        buttons_.push_back(b);
    };

    // Navigation row.
    const int shown = reviewing() ? viewPly_ : game_.plyCount();
    const float nw = (w - gap * 4.f) / 5.f;
    add(BtnFirst, {x, NAV_Y, nw, 40.f}, "", shown > 0, Icon::First);
    add(BtnPrev, {x + (nw + gap), NAV_Y, nw, 40.f}, "", shown > 0, Icon::Prev);
    add(BtnNext, {x + 2 * (nw + gap), NAV_Y, nw, 40.f}, "", reviewing(), Icon::Next);
    add(BtnLast, {x + 3 * (nw + gap), NAV_Y, nw, 40.f}, "", reviewing(), Icon::Last);
    add(BtnFlip, {x + 4 * (nw + gap), NAV_Y, nw, 40.f}, "", true, Icon::Flip);

    // Action grid (5 rows x 3 columns).
    const bool live = !game_.isOver();
    const bool myTurn = live && !isComputerTurn();
    const float bw = (w - gap * 2.f) / 3.f, bh = 38.f;
    auto cell = [&](int row, int col) {
        return sf::FloatRect(x + static_cast<float>(col) * (bw + gap), GRID_Y + static_cast<float>(row) * (bh + gap), bw, bh);
    };
    add(BtnHint, cell(0, 0), aiTask_ == AiTask::Hint ? "Thinking..." : "Hint", myTurn && aiTask_ == AiTask::None && !reviewing());
    add(BtnUndo, cell(0, 1), "Undo", game_.plyCount() > 0);
    add(BtnNewGame, cell(0, 2), "New game");
    add(BtnOfferDraw, cell(1, 0), "Offer draw", live && game_.plyCount() > 0);
    add(BtnClaimDraw, cell(1, 1), "Claim draw", (config_.vsComputer ? myTurn : live) && game_.claimableDraw() != EndReason::None);
    add(BtnResign, cell(1, 2), "Resign", live);
    add(BtnSave, cell(2, 0), "Save PGN");
    add(BtnLoad, cell(2, 1), "Load PGN");
    add(BtnCopyPgn, cell(2, 2), "Copy PGN");
    add(BtnCopyFen, cell(3, 0), "Copy FEN");
    add(BtnPaste, cell(3, 1), "Paste FEN/PGN");
    add(BtnSettings, cell(3, 2), "Settings");
    add(BtnMenu, cell(4, 0), "Main menu");
    add(BtnSound, cell(4, 1), ctx_.settings.sound ? "Sound: on" : "Sound: off");
    add(BtnTheme, cell(4, 2), std::string("Theme: ") + ctx_.theme().name);
}

sf::FloatRect GameScreen::moveCellRect(int ply) const {
    const bool startBlack = Board(game_.startFen()).sideToMove() == Color::Black;
    const int cellIndex = ply + (startBlack ? 1 : 0);
    const int row = cellIndex / 2, col = cellIndex % 2;
    return {layout::PANEL_X + 70.f + static_cast<float>(col) * 150.f,
            LIST_Y + 8.f + static_cast<float>(row - moveListScroll_) * ROW_H, 140.f, ROW_H - 3.f};
}

std::string GameScreen::statusLine() const {
    if (game_.isOver()) return game_.result().headline();
    if (reviewing()) return "Reviewing move " + std::to_string((viewPly_ + 1) / 2) + (viewPly_ % 2 == 1 ? "" : "...");
    const std::string side = game_.sideToMove() == Color::White ? "White" : "Black";
    if (isComputerTurn()) return "Computer is thinking...";
    if (config_.vsComputer) return "Your move (" + side + ")";
    return side + " to move";
}

void GameScreen::drawPlayerCard(sf::RenderTarget& target, Color color, bool top) {
    const Fonts& f = ctx_.fonts;
    const float y = top ? TOP_CARD_Y : BOTTOM_CARD_Y;
    const float x = layout::BOARD_X;
    const Board& board = displayedBoard();

    // Avatar: rounded square with a king of the player's colour.
    drawRoundedRect(target, {x, y, CARD_H, CARD_H}, 8.f,
                    color == Color::White ? sf::Color(225, 222, 216) : sf::Color(70, 67, 63));
    ctx_.renderer.drawPiece(target, Piece(config_.vsComputer && color != config_.humanColor ? PieceType::Knight : PieceType::King, color),
                            {x + 3.f, y + 3.f}, CARD_H - 6.f);

    // Name (+ level for the computer).
    float nx = x + CARD_H + 10.f;
    float w = drawText(target, f, playerName(color), {nx, y - 1.f}, 16, palette::Text, Weight::SemiBold);
    if (config_.vsComputer && color != config_.humanColor)
        drawText(target, f, "Level " + std::to_string(config_.aiLevel) + " \xC2\xB7 " + aiLevelName(config_.aiLevel),
                 {nx + w + 8.f, y + 1.f}, 14, palette::TextDim);

    // Captured pieces: what the opponent has lost compared with the start position.
    Board start(game_.startFen());
    const Color enemy = opposite(color);
    int startCount[7] = {0}, nowCount[7] = {0};
    int myMaterial = 0, enemyMaterial = 0;
    for (Square sq = 0; sq < 64; ++sq) {
        const Piece& s = start.at(sq);
        if (!s.isEmpty() && s.color == enemy) ++startCount[static_cast<int>(s.type)];
        const Piece& p = board.at(sq);
        if (p.isEmpty()) continue;
        if (p.color == enemy) {
            ++nowCount[static_cast<int>(p.type)];
            enemyMaterial += MATERIAL[static_cast<int>(p.type)];
        } else {
            myMaterial += MATERIAL[static_cast<int>(p.type)];
        }
    }
    float cx = nx;
    const float icon = 20.f;
    for (int t = 1; t <= 5; ++t) {  // pawns first, queen last
        int lost = std::max(0, startCount[t] - nowCount[t]);
        for (int i = 0; i < lost; ++i) {
            ctx_.renderer.drawPiece(target, Piece(static_cast<PieceType>(t), enemy), {cx, y + 19.f}, icon);
            cx += icon * 0.55f;
        }
        if (lost > 0) cx += icon * 0.5f;
    }
    if (myMaterial > enemyMaterial)
        drawText(target, f, "+" + std::to_string(myMaterial - enemyMaterial), {cx + 2.f, y + 20.f}, 14, palette::TextDim,
                 Weight::SemiBold);

    // Clock.
    if (clock_.enabled()) {
        const bool active = !game_.isOver() && game_.sideToMove() == color;
        const long long ms = clock_.remainingMs(color);
        const bool low = ms < 20000;
        sf::Color bg = active ? (low ? sf::Color(200, 60, 50) : sf::Color(235, 233, 228)) : palette::PanelDark;
        sf::Color fg = active ? (low ? palette::Text : sf::Color(30, 29, 27)) : (ms == 0 ? palette::Danger : palette::TextDim);
        sf::FloatRect r(layout::BOARD_X + layout::BOARD_SIZE - CLOCK_W, y, CLOCK_W, CARD_H);
        drawRoundedRect(target, r, 7.f, bg);
        drawText(target, f, ChessClock::format(ms), {r.left + r.width - 14.f, r.top + 6.f}, 23, fg, Weight::Bold,
                 Align::Right);
        if (active) {  // small "running" dot
            sf::CircleShape dot(4.f);
            dot.setPosition(r.left + 12.f, r.top + CARD_H / 2.f - 4.f);
            dot.setFillColor(low ? palette::Text : palette::Accent);
            target.draw(dot);
        }
    } else if (!game_.isOver() && game_.sideToMove() == color) {
        // Without a clock show whose turn it is.
        sf::FloatRect r(layout::BOARD_X + layout::BOARD_SIZE - 96.f, y + 8.f, 96.f, 24.f);
        drawRoundedRect(target, r, 12.f, palette::AccentDark);
        drawTextCentered(target, f, "to move", r, 13, palette::Text, Weight::SemiBold);
    }
}

void GameScreen::drawMoveList(sf::RenderTarget& target, sf::FloatRect area, sf::Vector2f mouse) {
    const Fonts& f = ctx_.fonts;
    drawRoundedRect(target, area, 10.f, palette::PanelDark);
    const auto& san = game_.sanHistory();
    const bool startBlack = Board(game_.startFen()).sideToMove() == Color::Black;
    const int firstMove = Board(game_.startFen()).fullmoveNumber();
    const int cells = static_cast<int>(san.size()) + (startBlack ? 1 : 0);
    const int rows = (cells + 1) / 2;
    const int visible = static_cast<int>((area.height - 16.f) / ROW_H);
    const int current = reviewing() ? viewPly_ - 1 : static_cast<int>(san.size()) - 1;

    if (followLatest_) moveListScroll_ = std::max(0, rows - visible);
    moveListScroll_ = std::max(0, std::min(moveListScroll_, std::max(0, rows - visible)));

    if (san.empty()) {
        drawText(target, f, "No moves yet", {area.left + area.width / 2.f, area.top + area.height / 2.f - 10.f}, 15,
                 palette::TextFaint, Weight::Regular, Align::Center);
    }
    for (int row = moveListScroll_; row < rows && row < moveListScroll_ + visible; ++row) {
        const float y = area.top + 8.f + static_cast<float>(row - moveListScroll_) * ROW_H;
        if (row % 2 == 1)
            drawRoundedRect(target, {area.left + 6.f, y, area.width - 12.f, ROW_H - 3.f}, 5.f, sf::Color(255, 255, 255, 6));
        drawText(target, f, std::to_string(firstMove + row) + ".", {area.left + 22.f, y + 5.f}, 15, palette::TextFaint);
        for (int col = 0; col < 2; ++col) {
            const int ply = row * 2 + col - (startBlack ? 1 : 0);
            if (ply < 0 || ply >= static_cast<int>(san.size())) continue;
            sf::FloatRect cell = moveCellRect(ply);
            if (ply == current) drawRoundedRect(target, cell, 5.f, palette::PanelLight);
            else if (cell.contains(mouse)) drawRoundedRect(target, cell, 5.f, sf::Color(255, 255, 255, 14));
            drawText(target, f, san[static_cast<std::size_t>(ply)], {cell.left + 10.f, y + 5.f}, 15,
                     ply == current ? palette::Text : sf::Color(205, 202, 197), ply == current ? Weight::Bold : Weight::SemiBold);
        }
    }
    // Scrollbar
    if (rows > visible) {
        float barH = area.height * static_cast<float>(visible) / static_cast<float>(rows);
        float barY = area.top + (area.height - barH) * static_cast<float>(moveListScroll_) / static_cast<float>(rows - visible);
        drawRoundedRect(target, {area.left + area.width - 8.f, barY + 4.f, 4.f, barH - 8.f}, 2.f, palette::TextFaint);
    }
    // Result at the end of the list
    if (game_.isOver() && rows < visible + moveListScroll_ + 1) {
        float y = area.top + 8.f + static_cast<float>(rows - moveListScroll_) * ROW_H;
        if (y < area.top + area.height - ROW_H)
            drawText(target, f, game_.result().pgnString() + "  " + game_.result().reasonText(), {area.left + 22.f, y + 5.f}, 15,
                     palette::Accent, Weight::SemiBold);
    }
}

void GameScreen::drawSidePanel(sf::RenderTarget& target, sf::Vector2f mouse) {
    const Fonts& f = ctx_.fonts;
    const float x = layout::PANEL_X, w = layout::PANEL_W;
    drawRoundedRect(target, {x, layout::PANEL_Y, w, layout::PANEL_H}, 14.f, palette::Panel);

    // Header: mode tag, status and details.
    std::string tag = config_.vsComputer ? "vs Computer" : "2 players";
    tag += "  \xC2\xB7  " + clock_.timeControl().toString();
    drawText(target, f, tag, {x + w - 18.f, HEADER_Y + 4.f}, 13, palette::TextFaint, Weight::SemiBold, Align::Right);
    const std::string status = statusLine();
    drawText(target, f, status, {x + 18.f, HEADER_Y}, 22, palette::Text, Weight::Bold);

    std::string detail;
    sf::Color detailColor = palette::TextDim;
    if (game_.isOver()) {
        detail = capitalise(game_.result().reasonText()) + "  (" + game_.result().pgnString() + ")";
        detailColor = palette::Accent;
    } else if (game_.board().inCheck() && !reviewing()) {
        detail = "Check!";
        detailColor = sf::Color(235, 90, 80);
    } else if (game_.claimableDraw() != EndReason::None) {
        detail = std::string("Draw can be claimed: ") +
                 (game_.claimableDraw() == EndReason::ThreefoldRepetition ? "threefold repetition" : "50-move rule");
        detailColor = sf::Color(230, 190, 90);
    } else if (!openingName_.empty()) {
        detail = openingName_;
    } else {
        detail = "Move " + std::to_string(game_.board().fullmoveNumber());
    }
    drawText(target, f, detail, {x + 18.f, HEADER_Y + 34.f}, 15, detailColor, Weight::SemiBold);
    if (config_.vsComputer && !aiInfo_.empty())
        drawText(target, f, aiInfo_, {x + w - 18.f, HEADER_Y + 58.f}, 13, palette::TextFaint, Weight::Regular, Align::Right);
    drawText(target, f, "Moves", {x + 18.f, HEADER_Y + 60.f}, 13, palette::TextFaint, Weight::SemiBold);

    drawMoveList(target, {x + 14.f, LIST_Y, w - 28.f, LIST_H}, mouse);
    for (const Button& b : buttons_) b.draw(target, f, mouse);
}

void GameScreen::draw(sf::RenderTarget& target, sf::Vector2f mouse) {
    rebuildButtons();
    const Board& board = displayedBoard();
    const int shown = reviewing() ? viewPly_ : game_.plyCount();

    BoardState s;
    s.board = &board;
    s.flipped = flipped_;
    s.theme = &ctx_.theme();
    s.showCoordinates = ctx_.settings.showCoordinates;
    s.showTargets = ctx_.settings.showLegalMoves;
    if (shown > 0) {
        const Move& last = game_.moves()[static_cast<std::size_t>(shown - 1)];
        s.lastFrom = last.from;
        s.lastTo = last.to;
    }
    s.selected = selected_;
    s.targets = targets_;
    s.dragging = dragging_;
    s.dragPos = dragPos_;
    s.hover = dragging_ ? Renderer::squareAt(mouse, flipped_) : NO_SQUARE;
    if (board.inCheck()) s.checkSquare = board.kingSquare(board.sideToMove());
    s.animation = animation_;
    if (hintFrom_ != NO_SQUARE && !reviewing()) s.arrows.push_back({hintFrom_, hintTo_, sf::Color(70, 160, 230, 190)});
    ctx_.renderer.drawBoard(target, ctx_.fonts, s);

    if (promoTo_ != NO_SQUARE)
        ctx_.renderer.drawPromotionPicker(target, promoTo_, game_.sideToMove(), flipped_, mouse);

    // Player cards: the side at the bottom of the board is shown below it.
    const Color bottom = flipped_ ? Color::Black : Color::White;
    drawPlayerCard(target, opposite(bottom), true);
    drawPlayerCard(target, bottom, false);

    drawSidePanel(target, mouse);

    // Review mode banner over the board.
    if (reviewing()) {
        sf::FloatRect r(layout::BOARD_X + layout::BOARD_SIZE / 2.f - 160.f, layout::BOARD_Y + layout::BOARD_SIZE - 48.f, 320.f, 34.f);
        drawRoundedRect(target, r, 17.f, sf::Color(20, 20, 20, 190));
        drawTextCentered(target, ctx_.fonts, "Review mode  \xC2\xB7  click the board to return", r, 14, palette::Text,
                         Weight::SemiBold);
    }

    // Toast message.
    if (toastTime_ > 0.f && !toast_.empty()) {
        const float alpha = std::min(1.f, toastTime_ / 0.4f);
        const float tw = textWidth(ctx_.fonts, toast_, 16, Weight::SemiBold) + 40.f;
        sf::FloatRect r(layout::BOARD_X + (layout::BOARD_SIZE - tw) / 2.f, layout::BOARD_Y + 14.f, tw, 42.f);
        drawRoundedRect(target, r, 21.f, sf::Color(25, 24, 22, static_cast<sf::Uint8>(225 * alpha)));
        drawTextCentered(target, ctx_.fonts, toast_, r, 16, sf::Color(240, 238, 234, static_cast<sf::Uint8>(255 * alpha)),
                         Weight::SemiBold);
    }

    if (dialog_ != Dialog::None) modal_.draw(target, ctx_.fonts, mouse);
}

}  // namespace gui
