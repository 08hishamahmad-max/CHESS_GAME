// Types.h - basic chess types shared by the whole engine.
//
// Squares are numbered 0..63 with a1 = 0, b1 = 1, ..., h1 = 7, a2 = 8, ..., h8 = 63.
// So   square = rank * 8 + file   (rank and file both 0..7).
#pragma once

#include <cstdint>
#include <string>

namespace chess {

enum class Color : int { White = 0, Black = 1 };

// Returns the other colour.
inline Color opposite(Color c) { return c == Color::White ? Color::Black : Color::White; }

enum class PieceType : int { None = 0, Pawn, Knight, Bishop, Rook, Queen, King };

// One square of the board: either empty (type == None) or a piece of some colour.
struct Piece {
    PieceType type = PieceType::None;
    Color color = Color::White;

    Piece() = default;
    Piece(PieceType t, Color c) : type(t), color(c) {}

    bool isEmpty() const { return type == PieceType::None; }
    bool is(PieceType t, Color c) const { return type == t && color == c; }
    bool operator==(const Piece& o) const {
        return type == o.type && (type == PieceType::None || color == o.color);
    }
    bool operator!=(const Piece& o) const { return !(*this == o); }
};

using Square = int;
constexpr Square NO_SQUARE = -1;

inline int fileOf(Square sq) { return sq & 7; }
inline int rankOf(Square sq) { return sq >> 3; }
inline Square makeSquare(int file, int rank) { return rank * 8 + file; }
inline bool onBoard(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

// "e4" style name of a square.
inline std::string squareName(Square sq) {
    if (sq < 0 || sq > 63) return "-";
    return std::string{char('a' + fileOf(sq)), char('1' + rankOf(sq))};
}

// Parses "e4" -> square index, or NO_SQUARE if the text is not a square.
inline Square parseSquare(const std::string& s) {
    if (s.size() != 2 || s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8') return NO_SQUARE;
    return makeSquare(s[0] - 'a', s[1] - '1');
}

// Upper-case letter used in algebraic notation / FEN (P, N, B, R, Q, K).
inline char pieceLetter(PieceType t) {
    switch (t) {
        case PieceType::Pawn:   return 'P';
        case PieceType::Knight: return 'N';
        case PieceType::Bishop: return 'B';
        case PieceType::Rook:   return 'R';
        case PieceType::Queen:  return 'Q';
        case PieceType::King:   return 'K';
        default:                return '?';
    }
}

// Castling right bit flags.
enum CastlingRights : int {
    NO_CASTLING = 0,
    WHITE_KINGSIDE = 1,
    WHITE_QUEENSIDE = 2,
    BLACK_KINGSIDE = 4,
    BLACK_QUEENSIDE = 8,
    ALL_CASTLING = 15
};

}  // namespace chess
