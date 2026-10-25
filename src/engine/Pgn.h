// Pgn.h - reading and writing games in PGN (Portable Game Notation), the
// standard text format used by every chess program and website.
//
// Example:
//   [Event "Casual game"]
//   [White "Hisham"]
//   [Black "Computer"]
//   [Result "1-0"]
//
//   1. e4 e5 2. Qh5 Nc6 3. Bc4 Nf6 4. Qxf7# 1-0
#pragma once

#include "Game.h"

#include <string>
#include <utility>
#include <vector>

namespace chess {

using PgnTags = std::vector<std::pair<std::string, std::string>>;

namespace pgn {

// Writes the game as PGN. `tags` are written first (in order); the Result
// tag and, for games not starting from the normal position, SetUp/FEN are
// added automatically.
std::string write(const Game& game, const PgnTags& tags);

// Reads the first game of a PGN text into `game`. Comments {...}, ;...,
// variations (...), NAGs ($1) and move numbers are skipped.
// Returns false and fills `error` if a move is illegal or the text is not PGN.
bool read(const std::string& text, Game& game, PgnTags* tagsOut, std::string& error);

// Value of a tag, or "" if missing.
std::string tagValue(const PgnTags& tags, const std::string& name);

}  // namespace pgn
}  // namespace chess
