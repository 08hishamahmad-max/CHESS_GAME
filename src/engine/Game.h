// Game.h - a whole game of chess: the current board, the move history (with
// algebraic notation), undo, and the game result (checkmate, draws,
// resignation, time forfeit, ...).
//
// The GUI talks to Game; Game uses Board, MoveGenerator and Notation.
#pragma once

#include "Board.h"
#include "Move.h"

#include <cstdint>
#include <string>
#include <vector>

namespace chess {

enum class Outcome { Ongoing, WhiteWins, BlackWins, Draw };

enum class EndReason {
    None,
    Checkmate,
    Stalemate,
    InsufficientMaterial,
    FiftyMoveRule,          // claimed by a player
    SeventyFiveMoveRule,    // automatic
    ThreefoldRepetition,    // claimed by a player
    FivefoldRepetition,     // automatic
    Agreement,
    Resignation,
    Timeout,
    TimeoutVsInsufficientMaterial,  // flag fell, but the opponent cannot mate -> draw
    Imported                        // result read from a PGN file
};

struct GameResult {
    Outcome outcome = Outcome::Ongoing;
    EndReason reason = EndReason::None;

    bool isOver() const { return outcome != Outcome::Ongoing; }
    std::string pgnString() const;     // "1-0", "0-1", "1/2-1/2" or "*"
    std::string headline() const;      // "White wins", "Draw", ...
    std::string reasonText() const;    // "by checkmate", "by threefold repetition", ...
};

class Game {
public:
    explicit Game(const std::string& fen = Board::START_FEN);

    // Starts a new game from a FEN position. Returns false (and keeps the
    // current game) if the FEN is invalid.
    bool newGame(const std::string& fen = Board::START_FEN);

    const Board& board() const { return board_; }
    const std::string& startFen() const { return startFen_; }
    Color sideToMove() const { return board_.sideToMove(); }
    const GameResult& result() const { return result_; }
    bool isOver() const { return result_.isOver(); }

    // Legal moves of the current position (recomputed after each move).
    const std::vector<Move>& legalMoves() const { return legalMoves_; }
    std::vector<Move> legalMovesFrom(Square from) const;

    // Plays a move if it is legal. Only from/to/promotion of `m` are compared,
    // so the caller does not need to know the special-move flag.
    bool makeMove(const Move& m);
    bool makeSanMove(const std::string& san);  // e.g. "Nf3"

    // Takes back the last move (also re-opens a finished game).
    bool undo();

    // History
    const std::vector<Move>& moves() const { return moves_; }
    const std::vector<std::string>& sanHistory() const { return san_; }
    int plyCount() const { return static_cast<int>(moves_.size()); }
    Move lastMove() const { return moves_.empty() ? Move{} : moves_.back(); }
    // The position after `ply` half-moves (0 = start position).
    Board boardAtPly(int ply) const;
    // Position keys since the last capture or pawn move (for the AI's repetition detection).
    std::vector<std::uint64_t> recentPositionKeys() const;

    // --- draws, resignation, time --------------------------------------------
    // Threefold repetition and the 50-move rule must be *claimed* (FIDE rules);
    // fivefold repetition and the 75-move rule end the game automatically.
    EndReason claimableDraw() const;  // None if no draw can be claimed now
    bool claimDraw();
    void resign(Color loser);
    void agreeDraw();
    void flagFall(Color loser);       // loser ran out of time
    void setImportedResult(Outcome outcome);

    // Can colour `c` still checkmate with some sequence of legal moves?
    // (Simplified: false only for a lone king or king + one minor piece.)
    bool hasMatingMaterial(Color c) const;

private:
    void refresh();  // recompute legal moves and rule-based result
    bool isInsufficientMaterial() const;
    int repetitionCount() const;

    Board board_;
    std::string startFen_;
    std::vector<Move> moves_;
    std::vector<std::string> san_;
    std::vector<std::uint64_t> keys_;  // position key after every ply (index 0 = start)
    std::vector<Move> legalMoves_;
    GameResult result_;
};

}  // namespace chess
