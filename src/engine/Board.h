// Board.h - the chess position and the ability to make / unmake moves.
//
// The board is a simple "mailbox": an array of 64 Piece values. Besides the
// pieces, a position needs: side to move, castling rights, en passant square,
// the half-move clock (for the 50-move rule) and the full-move number.
// These are exactly the six fields of a FEN string.
#pragma once

#include "Move.h"
#include "Types.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chess {

class Board {
public:
    static const char* const START_FEN;

    Board();                                  // starting position
    explicit Board(const std::string& fen);   // any position

    // --- set-up -------------------------------------------------------------
    bool loadFen(const std::string& fen);  // returns false (board unchanged) on an invalid FEN
    std::string toFen() const;
    void clear();

    // --- queries ------------------------------------------------------------
    const Piece& at(Square sq) const { return squares_[sq]; }
    Color sideToMove() const { return sideToMove_; }
    int castlingRights() const { return castling_; }
    Square enPassantSquare() const { return epSquare_; }
    int halfmoveClock() const { return halfmoveClock_; }
    int fullmoveNumber() const { return fullmoveNumber_; }
    Square kingSquare(Color c) const { return kingSquare_[static_cast<int>(c)]; }

    // Is square `sq` attacked by any piece of colour `by`?
    bool isSquareAttacked(Square sq, Color by) const;
    // Is the king of colour `c` currently in check?
    bool inCheck(Color c) const;
    bool inCheck() const { return inCheck(sideToMove_); }

    // 64-bit Zobrist key identifying the position (pieces, side to move,
    // castling rights and a *capturable* en passant square). Two positions are
    // "the same" for the threefold repetition rule iff their keys are equal.
    std::uint64_t positionKey() const;

    // --- making moves -------------------------------------------------------
    // makeMove does NOT check legality (MoveGenerator does that). Every
    // makeMove must be matched by an unmakeMove, which restores the exact
    // previous position using the information saved on an internal stack.
    void makeMove(const Move& m);
    void unmakeMove();

    // Puts a piece on a square (also keeps the king-square cache up to date).
    void setPiece(Square sq, Piece p);

private:
    struct NoInit {};
    explicit Board(NoInit) {}  // empty board, used internally by loadFen

    // Everything makeMove destroys and unmakeMove needs to restore.
    struct UndoInfo {
        Move move;
        Piece captured;
        int castling;
        Square epSquare;
        int halfmoveClock;
        int fullmoveNumber;
    };

    std::array<Piece, 64> squares_{};
    Color sideToMove_ = Color::White;
    int castling_ = ALL_CASTLING;
    Square epSquare_ = NO_SQUARE;
    int halfmoveClock_ = 0;
    int fullmoveNumber_ = 1;
    std::array<Square, 2> kingSquare_{{NO_SQUARE, NO_SQUARE}};
    std::vector<UndoInfo> history_;
};

}  // namespace chess
