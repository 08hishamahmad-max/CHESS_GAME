// OpeningBook.h - a small built-in opening book.
//
// The book is a list of well-known opening lines written in coordinate
// notation. At start-up every line is replayed and each position (Zobrist
// key) is mapped to the book moves played from it. The computer picks a random
// book move while the game is still "in the book", which makes its openings
// varied and sensible. The book also knows the name of each opening.
#pragma once

#include "Board.h"
#include "Move.h"

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace chess {

class OpeningBook {
public:
    // The single shared book (built on first use).
    static const OpeningBook& instance();

    // A random book move for this position, or a null move if out of book.
    Move probe(const Board& board, std::mt19937& rng) const;

    // All book moves for this position.
    std::vector<Move> movesFor(const Board& board) const;

    // Name of the opening for a game from the standard start position that
    // has played these moves (longest matching line), or "" if unknown.
    std::string openingName(const std::vector<Move>& moves) const;

    // Number of lines whose moves were all legal (used by the unit tests).
    int validLineCount() const { return validLines_; }
    int totalLineCount() const;

private:
    OpeningBook();

    struct Line {
        std::string name;
        std::vector<std::string> uci;
    };
    std::vector<Line> lines_;
    std::unordered_map<std::uint64_t, std::vector<Move>> positions_;
    int validLines_ = 0;
};

}  // namespace chess
