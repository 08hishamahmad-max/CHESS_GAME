// MoveGenerator.h - generates the legal moves of a position.
//
// Strategy (easy to explain and verify with perft):
//   1. generate all *pseudo-legal* moves: moves that follow the movement rules
//      of each piece but may leave the own king in check;
//   2. for each one, make it on the board, reject it if our king is attacked,
//      and unmake it again.
// Castling is checked completely in step 1 (king not in check, path empty,
// king does not cross an attacked square).
#pragma once

#include "Board.h"
#include "Move.h"

#include <cstdint>
#include <vector>

namespace chess {

class MoveGenerator {
public:
    // All legal moves for the side to move. The board is temporarily modified
    // (make/unmake) but is returned unchanged.
    static std::vector<Move> legalMoves(Board& board);

    // Legal moves of the piece standing on `from` only (used by the GUI).
    static std::vector<Move> legalMovesFrom(Board& board, Square from);

    // Pseudo-legal moves (may leave the king in check).
    static void pseudoLegalMoves(const Board& board, std::vector<Move>& out);

    // Counts leaf nodes of the legal move tree to the given depth.
    // Standard correctness test for move generators.
    static std::uint64_t perft(Board& board, int depth);

private:
    static void addPawnMoves(const Board& board, Square from, std::vector<Move>& out);
    static void addPieceMoves(const Board& board, Square from, std::vector<Move>& out);
    static void addCastlingMoves(const Board& board, std::vector<Move>& out);
};

}  // namespace chess
