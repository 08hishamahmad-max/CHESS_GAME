// OpeningBook.cpp
#include "OpeningBook.h"

#include "Notation.h"

#include <algorithm>
#include <sstream>

namespace chess {

namespace {

struct RawLine {
    const char* name;
    const char* moves;
};

// Main lines of popular openings (coordinate notation, from the start position).
const RawLine BOOK[] = {
    {"Ruy Lopez: Closed", "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 c2c3 e8g8"},
    {"Ruy Lopez: Berlin Defence", "e2e4 e7e5 g1f3 b8c6 f1b5 g8f6 e1g1 f6e4 d2d4 e4d6 b5c6 d7c6 d4e5 d6f5"},
    {"Italian Game: Giuoco Piano", "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d3 d7d6 e1g1 e8g8"},
    {"Italian Game: Two Knights Defence", "e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 d2d3 f8e7 e1g1 e8g8"},
    {"Italian Game: Evans Gambit", "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 b2b4 c5b4 c2c3 b4a5 d2d4"},
    {"Scotch Game", "e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 g8f6 d4c6 b7c6 e4e5 d8e7"},
    {"Petrov's Defence", "e2e4 e7e5 g1f3 g8f6 f3e5 d7d6 e5f3 f6e4 d2d4 d6d5 f1d3"},
    {"Four Knights Game", "e2e4 e7e5 g1f3 b8c6 b1c3 g8f6 f1b5 f8b4 e1g1 e8g8"},
    {"Philidor Defence", "e2e4 e7e5 g1f3 d7d6 d2d4 g8f6 b1c3 b8d7 f1c4 f8e7 e1g1 e8g8"},
    {"King's Gambit Accepted", "e2e4 e7e5 f2f4 e5f4 g1f3 g7g5 h2h4 g5g4 f3e5"},
    {"Vienna Game", "e2e4 e7e5 b1c3 g8f6 f2f4 d7d5 f4e5 f6e4 g1f3"},
    {"Sicilian Defence: Najdorf", "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1e3 e7e5 d4b3 c8e6"},
    {"Sicilian Defence: Dragon", "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 g7g6 c1e3 f8g7 f2f3 e8g8"},
    {"Sicilian Defence: Sveshnikov", "e2e4 c7c5 g1f3 b8c6 d2d4 c5d4 f3d4 g8f6 b1c3 e7e5 d4b5 d7d6"},
    {"Sicilian Defence: Taimanov", "e2e4 c7c5 g1f3 e7e6 d2d4 c5d4 f3d4 b8c6 b1c3 d8c7"},
    {"Sicilian Defence: Alapin", "e2e4 c7c5 c2c3 g8f6 e4e5 f6d5 d2d4 c5d4 g1f3 b8c6"},
    {"French Defence: Winawer", "e2e4 e7e6 d2d4 d7d5 b1c3 f8b4 e4e5 c7c5 a2a3 b4c3 b2c3 g8e7"},
    {"French Defence: Advance", "e2e4 e7e6 d2d4 d7d5 e4e5 c7c5 c2c3 b8c6 g1f3 d8b6"},
    {"French Defence: Tarrasch", "e2e4 e7e6 d2d4 d7d5 b1d2 g8f6 e4e5 f6d7 f1d3 c7c5 c2c3 b8c6"},
    {"Caro-Kann Defence: Classical", "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6 h2h4 h7h6"},
    {"Caro-Kann Defence: Advance", "e2e4 c7c6 d2d4 d7d5 e4e5 c8f5 g1f3 e7e6 f1e2 c6c5"},
    {"Scandinavian Defence", "e2e4 d7d5 e4d5 d8d5 b1c3 d5a5 d2d4 g8f6 g1f3 c8f5"},
    {"Pirc Defence", "e2e4 d7d6 d2d4 g8f6 b1c3 g7g6 g1f3 f8g7 f1e2 e8g8 e1g1"},
    {"Alekhine's Defence", "e2e4 g8f6 e4e5 f6d5 d2d4 d7d6 g1f3 c8g4 f1e2 e7e6"},
    {"Queen's Gambit Declined", "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 e8g8 g1f3 h7h6"},
    {"Queen's Gambit Accepted", "d2d4 d7d5 c2c4 d5c4 g1f3 g8f6 e2e3 e7e6 f1c4 c7c5 e1g1 a7a6"},
    {"Slav Defence", "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 d5c4 a2a4 c8f5 e2e3 e7e6"},
    {"Semi-Slav Defence: Meran", "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 e7e6 e2e3 b8d7 f1d3 d5c4 d3c4 b7b5"},
    {"London System", "d2d4 d7d5 c1f4 g8f6 e2e3 c7c5 c2c3 b8c6 b1d2 e7e6 g1f3 f8d6"},
    {"King's Indian Defence", "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e8g8 f1e2 e7e5 e1g1 b8c6"},
    {"Grunfeld Defence", "d2d4 g8f6 c2c4 g7g6 b1c3 d7d5 c4d5 f6d5 e2e4 d5c3 b2c3 f8g7"},
    {"Nimzo-Indian Defence", "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 e2e3 e8g8 f1d3 d7d5 g1f3 c7c5"},
    {"Queen's Indian Defence", "d2d4 g8f6 c2c4 e7e6 g1f3 b7b6 g2g3 c8b7 f1g2 f8e7 e1g1 e8g8"},
    {"Catalan Opening", "d2d4 g8f6 c2c4 e7e6 g2g3 d7d5 f1g2 f8e7 g1f3 e8g8 e1g1 d5c4"},
    {"Modern Benoni", "d2d4 g8f6 c2c4 c7c5 d4d5 e7e6 b1c3 e6d5 c4d5 d7d6 e2e4 g7g6"},
    {"Dutch Defence", "d2d4 f7f5 g2g3 g8f6 f1g2 e7e6 g1f3 f8e7 e1g1 e8g8 c2c4 d7d6"},
    {"English Opening: Four Knights", "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 g2g3 d7d5 c4d5 f6d5 f1g2 d5b6"},
    {"English Opening: Symmetrical", "c2c4 c7c5 g1f3 b8c6 b1c3 g7g6 g2g3 f8g7 f1g2 g8f6 e1g1 e8g8"},
    {"Reti Opening", "g1f3 d7d5 g2g3 g8f6 f1g2 e7e6 e1g1 f8e7 d2d3 e8g8"},
    {"Bird's Opening", "f2f4 d7d5 g1f3 g8f6 e2e3 g7g6 f1e2 f8g7 e1g1 e8g8"},
};

}  // namespace

const OpeningBook& OpeningBook::instance() {
    static const OpeningBook book;  // built once, thread-safe since C++11
    return book;
}

int OpeningBook::totalLineCount() const { return static_cast<int>(sizeof(BOOK) / sizeof(BOOK[0])); }

OpeningBook::OpeningBook() {
    for (const RawLine& raw : BOOK) {
        Line line{raw.name, {}};
        std::istringstream in(raw.moves);
        std::string token;
        while (in >> token) line.uci.push_back(token);

        // Replay the line, remembering which move was played in each position.
        Board board;
        bool valid = true;
        for (const std::string& uci : line.uci) {
            Move m = notation::fromUci(board, uci);
            if (m.isNull()) {
                valid = false;  // typo in the book: ignore the rest of the line
                break;
            }
            std::vector<Move>& list = positions_[board.positionKey()];
            if (std::find(list.begin(), list.end(), m) == list.end()) list.push_back(m);
            board.makeMove(m);
        }
        if (valid) ++validLines_;
        lines_.push_back(line);
    }
}

std::vector<Move> OpeningBook::movesFor(const Board& board) const {
    auto it = positions_.find(board.positionKey());
    return it == positions_.end() ? std::vector<Move>{} : it->second;
}

Move OpeningBook::probe(const Board& board, std::mt19937& rng) const {
    std::vector<Move> moves = movesFor(board);
    if (moves.empty()) return Move{};
    std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
    return moves[pick(rng)];
}

std::string OpeningBook::openingName(const std::vector<Move>& moves) const {
    // Find the lines that follow the game for the most plies.
    std::size_t bestLength = 0;
    std::vector<const Line*> best;
    for (const Line& line : lines_) {
        std::size_t k = 0;
        while (k < moves.size() && k < line.uci.size() && moves[k].toUci() == line.uci[k]) ++k;
        if (k == 0 || k < bestLength) continue;
        if (k > bestLength) best.clear();
        bestLength = k;
        best.push_back(&line);
    }
    if (best.empty() || bestLength < 2) return "";
    if (best.size() == 1) return best.front()->name;
    // Several lines match equally well: use their common family name
    // ("Sicilian Defence: Najdorf" -> "Sicilian Defence") if they share one.
    auto family = [](const std::string& name) { return name.substr(0, name.find(':')); };
    const std::string f = family(best.front()->name);
    for (const Line* l : best)
        if (family(l->name) != f) return "";
    return f;
}

}  // namespace chess
