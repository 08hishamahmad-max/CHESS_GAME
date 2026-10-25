// perft.cpp - move generator correctness test (console program, no GUI).
//
// "perft" (performance test) counts all leaf nodes of the legal move tree to a
// given depth. The correct numbers for well-known positions are published, so
// if our counts match, castling, en passant, promotion and check handling are
// all implemented correctly.
//
// Run:  ./perft        (exit code 0 = all tests passed)
// The other rule tests (SAN, FEN, PGN, draws, clock, AI) are in unit_tests.cpp.
#include "engine/Board.h"
#include "engine/MoveGenerator.h"

#include <chrono>
#include <cstdint>
#include <iostream>

using namespace chess;

namespace {

struct PerftCase {
    const char* name;
    const char* fen;
    int depth;
    std::uint64_t expected;
};

const PerftCase CASES[] = {
    {"Start position", Board::START_FEN, 1, 20},
    {"Start position", Board::START_FEN, 2, 400},
    {"Start position", Board::START_FEN, 3, 8902},
    {"Start position", Board::START_FEN, 4, 197281},
    {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48},
    {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039},
    {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862},
    {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603},
    // Extra well-known positions (en passant / promotion edge cases).
    {"Position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238},
    {"Position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624},
    {"Position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467},
    {"Position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379},
};

}  // namespace

int main() {
    int failures = 0;
    std::cout << "Perft tests\n";
    for (const PerftCase& c : CASES) {
        Board board(c.fen);
        auto t0 = std::chrono::steady_clock::now();
        std::uint64_t nodes = MoveGenerator::perft(board, c.depth);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        bool ok = nodes == c.expected;
        if (!ok) ++failures;
        std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] " << c.name << " depth " << c.depth << ": " << nodes
                  << " (expected " << c.expected << ", " << ms << " ms)\n";
    }

    std::cout << '\n' << (failures == 0 ? "All tests passed." : "Some tests FAILED.") << '\n';
    return failures == 0 ? 0 : 1;
}
