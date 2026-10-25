// Search.h - the chess engine ("AI") that chooses the computer's moves.
//
// Techniques used (all classic, see the README for explanations):
//   * negamax search with alpha-beta pruning
//   * iterative deepening (search depth 1, 2, 3, ... until time runs out)
//   * quiescence search (captures only) at the leaves
//   * transposition table indexed by Zobrist keys
//   * move ordering: hash move, MVV-LVA captures, killer moves, history heuristic
//   * check extension, repetition / 50-move draw detection
//   * evaluation: material, piece-square tables (tapered between middlegame
//     and endgame), bishop pair, pawn structure, rook on open file, and a
//     "mop-up" term that helps to checkmate a lone king
//   * opening book for the first moves
//   * optional randomness for the weaker levels
#pragma once

#include "Board.h"
#include "Move.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <vector>

namespace chess {

struct SearchLimits {
    int maxDepth = 64;        // iterative deepening stops at this depth
    int timeMs = 1000;        // time budget for this move (milliseconds)
    int randomMarginCp = 0;   // >0: pick randomly among moves at most this much worse
    bool useBook = true;      // play opening book moves when available
};

struct SearchResult {
    Move best;
    int score = 0;            // centipawns from the side to move's point of view
    int depth = 0;            // last fully completed depth
    std::uint64_t nodes = 0;
    bool fromBook = false;
    bool isMateScore() const;
    int mateInMoves() const;  // >0: we mate in N, <0: we get mated in N, 0: no mate
};

class Search {
public:
    static constexpr int INF = 1000000;
    static constexpr int MATE = 100000;

    Search();

    // Finds the best move for the side to move. `history` are the keys of the
    // positions that occurred before (for repetition detection), `stop` can be
    // set from another thread to abort the search early.
    SearchResult think(const Board& position, const SearchLimits& limits,
                       const std::vector<std::uint64_t>& history = {},
                       const std::atomic<bool>* stop = nullptr);

    // Static evaluation in centipawns from the side to move's point of view.
    static int evaluate(const Board& board);

    void clearTranspositionTable();

private:
    static constexpr int MAX_PLY = 96;

    enum class Bound : std::uint8_t { Exact, Lower, Upper };
    struct TTEntry {
        std::uint64_t key = 0;
        Move move;
        int score = 0;
        std::int16_t depth = -1;
        Bound bound = Bound::Exact;
    };

    int negamax(Board& board, int depth, int alpha, int beta, int ply);
    int quiescence(Board& board, int alpha, int beta, int ply);
    void orderMoves(const Board& board, std::vector<Move>& moves, const Move& ttMove, int ply) const;
    bool isRepetition(std::uint64_t key, int halfmoveClock) const;
    void checkTime();

    std::vector<TTEntry> table_;
    Move killers_[MAX_PLY][2];
    int history_[64][64];
    std::vector<std::uint64_t> pathKeys_;  // game history + current search path

    std::chrono::steady_clock::time_point startTime_;
    int timeLimitMs_ = 0;
    const std::atomic<bool>* stopFlag_ = nullptr;
    bool stopped_ = false;
    std::uint64_t nodes_ = 0;
    std::mt19937 rng_;
};

}  // namespace chess
