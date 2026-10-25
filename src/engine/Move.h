// Move.h - a single chess move.
#pragma once

#include "Types.h"

#include <string>

namespace chess {

// Special move kinds. A normal move (quiet or capture) uses Normal.
enum class MoveFlag : int {
    Normal = 0,
    DoublePawnPush,  // pawn moves two squares (creates an en passant square)
    EnPassant,       // pawn captures en passant
    CastleKingside,  // O-O
    CastleQueenside  // O-O-O
};

struct Move {
    Square from = NO_SQUARE;
    Square to = NO_SQUARE;
    PieceType promotion = PieceType::None;  // piece a pawn turns into (or None)
    MoveFlag flag = MoveFlag::Normal;

    Move() = default;
    Move(Square f, Square t, MoveFlag fl = MoveFlag::Normal, PieceType promo = PieceType::None)
        : from(f), to(t), promotion(promo), flag(fl) {}

    bool isNull() const { return from == NO_SQUARE; }
    bool isPromotion() const { return promotion != PieceType::None; }
    bool isCastle() const {
        return flag == MoveFlag::CastleKingside || flag == MoveFlag::CastleQueenside;
    }

    bool operator==(const Move& o) const {
        return from == o.from && to == o.to && promotion == o.promotion && flag == o.flag;
    }
    bool operator!=(const Move& o) const { return !(*this == o); }

    // Coordinate notation, e.g. "e2e4" or "e7e8q" (UCI style). Useful for debugging.
    std::string toUci() const;
};

}  // namespace chess
