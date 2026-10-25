// Notation.cpp
#include "Notation.h"

#include "MoveGenerator.h"

#include <cctype>

namespace chess {
namespace notation {

std::string toSan(Board& board, const Move& m) {
    return toSan(board, m, MoveGenerator::legalMoves(board));
}

std::string toSan(Board& board, const Move& m, const std::vector<Move>& legalMoves) {
    std::string san;
    const Piece piece = board.at(m.from);

    if (m.flag == MoveFlag::CastleKingside) {
        san = "O-O";
    } else if (m.flag == MoveFlag::CastleQueenside) {
        san = "O-O-O";
    } else {
        const bool capture = !board.at(m.to).isEmpty() || m.flag == MoveFlag::EnPassant;
        if (piece.type == PieceType::Pawn) {
            if (capture) san += char('a' + fileOf(m.from));  // e.g. "exd5"
        } else {
            san += pieceLetter(piece.type);
            // Disambiguation: can another piece of the same kind reach the square?
            bool ambiguous = false, sameFile = false, sameRank = false;
            for (const Move& other : legalMoves) {
                if (other.to != m.to || other.from == m.from) continue;
                if (board.at(other.from).type != piece.type) continue;
                ambiguous = true;
                if (fileOf(other.from) == fileOf(m.from)) sameFile = true;
                if (rankOf(other.from) == rankOf(m.from)) sameRank = true;
            }
            if (ambiguous) {
                if (!sameFile)
                    san += char('a' + fileOf(m.from));  // "Nbd2"
                else if (!sameRank)
                    san += char('1' + rankOf(m.from));  // "R1e2"
                else
                    san += squareName(m.from);          // "Qh4e1"
            }
        }
        if (capture) san += 'x';
        san += squareName(m.to);
        if (m.isPromotion()) {
            san += '=';
            san += pieceLetter(m.promotion);
        }
    }

    // Check (+) or checkmate (#): play the move and look.
    board.makeMove(m);
    if (board.inCheck()) san += MoveGenerator::legalMoves(board).empty() ? '#' : '+';
    board.unmakeMove();
    return san;
}

namespace {

// Removes check marks, annotations and "=" so that "e8=Q+!" -> "e8Q",
// and turns zeros in castling into letters ("0-0" -> "OO").
std::string normalise(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '+' || c == '#' || c == '!' || c == '?' || c == '=' || c == '-') continue;
        if (c == '0') c = 'O';
        out += c;
    }
    return out;
}

}  // namespace

Move fromSan(Board& board, const std::string& san) {
    const std::string wanted = normalise(san);
    if (wanted.empty()) return Move{};
    const std::vector<Move> legal = MoveGenerator::legalMoves(board);
    for (const Move& m : legal)
        if (normalise(toSan(board, m, legal)) == wanted) return m;

    // Accept lower-case promotion letters ("e8q") and a redundant "P" prefix.
    std::string alt = wanted;
    if (!alt.empty() && alt[0] == 'P') alt.erase(0, 1);
    if (!alt.empty()) alt.back() = static_cast<char>(std::toupper(static_cast<unsigned char>(alt.back())));
    for (const Move& m : legal)
        if (normalise(toSan(board, m, legal)) == alt) return m;

    // Over-specified moves such as "Ng1f3": compare as coordinates.
    if (wanted.size() >= 5 && std::isupper(static_cast<unsigned char>(wanted[0]))) {
        Square from = parseSquare(wanted.substr(1, 2));
        Square to = parseSquare(wanted.substr(wanted.size() - 2));
        if (wanted[3] == 'x') to = parseSquare(wanted.substr(4, 2));
        for (const Move& m : legal)
            if (m.from == from && m.to == to && board.at(m.from).type != PieceType::Pawn) return m;
    }
    return Move{};
}

Move fromUci(Board& board, const std::string& uci) {
    if (uci.size() < 4) return Move{};
    Square from = parseSquare(uci.substr(0, 2));
    Square to = parseSquare(uci.substr(2, 2));
    PieceType promo = PieceType::None;
    if (uci.size() >= 5) {
        switch (std::tolower(static_cast<unsigned char>(uci[4]))) {
            case 'q': promo = PieceType::Queen; break;
            case 'r': promo = PieceType::Rook; break;
            case 'b': promo = PieceType::Bishop; break;
            case 'n': promo = PieceType::Knight; break;
            default: break;
        }
    }
    for (const Move& m : MoveGenerator::legalMoves(board))
        if (m.from == from && m.to == to && m.promotion == promo) return m;
    return Move{};
}

}  // namespace notation
}  // namespace chess
