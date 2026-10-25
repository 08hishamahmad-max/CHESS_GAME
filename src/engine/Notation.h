// Notation.h - converting moves to and from text.
//
//  * SAN (Standard Algebraic Notation): "e4", "Nbd2", "exd6", "O-O", "e8=Q+", "Qh4#"
//  * UCI / coordinate notation: "e2e4", "e7e8q"
#pragma once

#include "Board.h"
#include "Move.h"

#include <string>
#include <vector>

namespace chess {
namespace notation {

// SAN of a legal move in the given position. The board is temporarily
// modified (to find out whether the move gives check/mate) and restored.
std::string toSan(Board& board, const Move& move);

// Same, but re-uses an already generated list of legal moves (faster).
std::string toSan(Board& board, const Move& move, const std::vector<Move>& legalMoves);

// Finds the legal move written in SAN. Tolerant: accepts "0-0", missing or
// extra "+"/"#", annotations like "!?" and "e8Q" instead of "e8=Q".
// Returns a null move if no legal move matches.
Move fromSan(Board& board, const std::string& san);

// Finds the legal move written in UCI notation ("e2e4", "e7e8q"), or null.
Move fromUci(Board& board, const std::string& uci);

}  // namespace notation
}  // namespace chess
