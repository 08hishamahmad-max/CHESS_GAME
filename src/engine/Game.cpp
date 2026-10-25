// Game.cpp
#include "Game.h"

#include "MoveGenerator.h"
#include "Notation.h"

#include <algorithm>

namespace chess {

// ---------------------------------------------------------------------------
// GameResult texts
// ---------------------------------------------------------------------------
std::string GameResult::pgnString() const {
    switch (outcome) {
        case Outcome::WhiteWins: return "1-0";
        case Outcome::BlackWins: return "0-1";
        case Outcome::Draw:      return "1/2-1/2";
        default:                 return "*";
    }
}

std::string GameResult::headline() const {
    switch (outcome) {
        case Outcome::WhiteWins: return "White wins";
        case Outcome::BlackWins: return "Black wins";
        case Outcome::Draw:      return "Draw";
        default:                 return "Game in progress";
    }
}

std::string GameResult::reasonText() const {
    switch (reason) {
        case EndReason::Checkmate:                     return "by checkmate";
        case EndReason::Stalemate:                     return "by stalemate";
        case EndReason::InsufficientMaterial:          return "by insufficient material";
        case EndReason::FiftyMoveRule:                 return "by the 50-move rule";
        case EndReason::SeventyFiveMoveRule:           return "by the 75-move rule";
        case EndReason::ThreefoldRepetition:           return "by threefold repetition";
        case EndReason::FivefoldRepetition:            return "by fivefold repetition";
        case EndReason::Agreement:                     return "by agreement";
        case EndReason::Resignation:                   return "by resignation";
        case EndReason::Timeout:                       return "on time";
        case EndReason::TimeoutVsInsufficientMaterial: return "timeout vs insufficient material";
        case EndReason::Imported:                      return "(result from PGN)";
        default:                                       return "";
    }
}

// ---------------------------------------------------------------------------
// Game
// ---------------------------------------------------------------------------
Game::Game(const std::string& fen) {
    if (!newGame(fen)) newGame(Board::START_FEN);
}

bool Game::newGame(const std::string& fen) {
    Board fresh;
    if (!fresh.loadFen(fen)) return false;
    board_ = fresh;
    startFen_ = board_.toFen();
    moves_.clear();
    san_.clear();
    keys_.assign(1, board_.positionKey());
    refresh();
    return true;
}

std::vector<Move> Game::legalMovesFrom(Square from) const {
    std::vector<Move> result;
    for (const Move& m : legalMoves_)
        if (m.from == from) result.push_back(m);
    return result;
}

bool Game::makeMove(const Move& m) {
    if (isOver()) return false;
    for (const Move& legal : legalMoves_) {
        if (legal.from == m.from && legal.to == m.to && legal.promotion == m.promotion) {
            san_.push_back(notation::toSan(board_, legal, legalMoves_));  // SAN is computed *before* moving
            board_.makeMove(legal);
            moves_.push_back(legal);
            keys_.push_back(board_.positionKey());
            refresh();
            return true;
        }
    }
    return false;
}

bool Game::makeSanMove(const std::string& san) {
    Move m = notation::fromSan(board_, san);
    return !m.isNull() && makeMove(m);
}

bool Game::undo() {
    if (moves_.empty()) return false;
    board_.unmakeMove();
    moves_.pop_back();
    san_.pop_back();
    keys_.pop_back();
    refresh();
    return true;
}

Board Game::boardAtPly(int ply) const {
    if (ply >= plyCount()) return board_;
    Board b(startFen_);
    for (int i = 0; i < ply && i < plyCount(); ++i) b.makeMove(moves_[i]);
    return b;
}

std::vector<std::uint64_t> Game::recentPositionKeys() const {
    std::vector<std::uint64_t> result;
    const int n = static_cast<int>(keys_.size());
    const int first = std::max(0, n - 1 - board_.halfmoveClock());
    for (int i = first; i < n; ++i) result.push_back(keys_[i]);
    return result;
}

void Game::refresh() {
    legalMoves_ = MoveGenerator::legalMoves(board_);
    result_ = GameResult{};

    if (legalMoves_.empty()) {
        // No legal moves: checkmate if in check, otherwise stalemate.
        if (board_.inCheck())
            result_ = {board_.sideToMove() == Color::White ? Outcome::BlackWins : Outcome::WhiteWins,
                       EndReason::Checkmate};
        else
            result_ = {Outcome::Draw, EndReason::Stalemate};
    } else if (isInsufficientMaterial()) {
        result_ = {Outcome::Draw, EndReason::InsufficientMaterial};
    } else if (board_.halfmoveClock() >= 150) {  // 75 moves by each side
        result_ = {Outcome::Draw, EndReason::SeventyFiveMoveRule};
    } else if (repetitionCount() >= 5) {
        result_ = {Outcome::Draw, EndReason::FivefoldRepetition};
    }
}

int Game::repetitionCount() const {
    // Only positions since the last capture / pawn move can repeat, which is
    // what the half-move clock tells us, so we look back at most that far.
    const std::uint64_t current = keys_.back();
    int count = 0;
    const int lookBack = board_.halfmoveClock();
    for (int i = static_cast<int>(keys_.size()) - 1, n = 0; i >= 0 && n <= lookBack; --i, ++n)
        if (keys_[i] == current) ++count;
    return count;
}

EndReason Game::claimableDraw() const {
    if (isOver()) return EndReason::None;
    if (repetitionCount() >= 3) return EndReason::ThreefoldRepetition;
    if (board_.halfmoveClock() >= 100) return EndReason::FiftyMoveRule;
    return EndReason::None;
}

bool Game::claimDraw() {
    EndReason r = claimableDraw();
    if (r == EndReason::None) return false;
    result_ = {Outcome::Draw, r};
    return true;
}

void Game::resign(Color loser) {
    if (isOver()) return;
    result_ = {loser == Color::White ? Outcome::BlackWins : Outcome::WhiteWins, EndReason::Resignation};
}

void Game::agreeDraw() {
    if (!isOver()) result_ = {Outcome::Draw, EndReason::Agreement};
}

void Game::flagFall(Color loser) {
    if (isOver()) return;
    if (!hasMatingMaterial(opposite(loser)))
        result_ = {Outcome::Draw, EndReason::TimeoutVsInsufficientMaterial};
    else
        result_ = {loser == Color::White ? Outcome::BlackWins : Outcome::WhiteWins, EndReason::Timeout};
}

void Game::setImportedResult(Outcome outcome) {
    if (!isOver() && outcome != Outcome::Ongoing) result_ = {outcome, EndReason::Imported};
}

bool Game::hasMatingMaterial(Color c) const {
    int minors = 0;
    for (Square sq = 0; sq < 64; ++sq) {
        const Piece& p = board_.at(sq);
        if (p.isEmpty() || p.color != c) continue;
        switch (p.type) {
            case PieceType::Pawn:
            case PieceType::Rook:
            case PieceType::Queen:
                return true;
            case PieceType::Knight:
            case PieceType::Bishop:
                ++minors;
                break;
            default:
                break;
        }
    }
    return minors >= 2;
}

bool Game::isInsufficientMaterial() const {
    // Dead position: neither side can ever checkmate.
    //   K vs K, K+minor vs K, and K+B(s) vs K+B(s) with all bishops on the
    //   same square colour.
    int knights = 0, bishops = 0;
    int bishopSquareColors = 0;  // bit 0: light-square bishop seen, bit 1: dark-square
    for (Square sq = 0; sq < 64; ++sq) {
        const Piece& p = board_.at(sq);
        switch (p.type) {
            case PieceType::Pawn:
            case PieceType::Rook:
            case PieceType::Queen:
                return false;
            case PieceType::Knight:
                ++knights;
                break;
            case PieceType::Bishop:
                ++bishops;
                bishopSquareColors |= ((fileOf(sq) + rankOf(sq)) % 2 == 0) ? 2 : 1;
                break;
            default:
                break;
        }
    }
    if (knights + bishops <= 1) return true;
    if (knights == 0 && bishopSquareColors != 3) return true;  // all bishops on one colour
    return false;
}

}  // namespace chess
