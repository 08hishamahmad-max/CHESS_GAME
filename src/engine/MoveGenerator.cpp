// MoveGenerator.cpp
#include "MoveGenerator.h"

namespace chess {

namespace {

const int KNIGHT_JUMPS[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
const int KING_STEPS[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
const int ROOK_DIRS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
const int BISHOP_DIRS[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

const PieceType PROMOTION_PIECES[4] = {PieceType::Queen, PieceType::Rook, PieceType::Bishop,
                                       PieceType::Knight};

}  // namespace

void MoveGenerator::pseudoLegalMoves(const Board& board, std::vector<Move>& out) {
    const Color us = board.sideToMove();
    for (Square sq = 0; sq < 64; ++sq) {
        const Piece& p = board.at(sq);
        if (p.isEmpty() || p.color != us) continue;
        if (p.type == PieceType::Pawn)
            addPawnMoves(board, sq, out);
        else
            addPieceMoves(board, sq, out);
    }
    addCastlingMoves(board, out);
}

void MoveGenerator::addPawnMoves(const Board& board, Square from, std::vector<Move>& out) {
    const Color us = board.sideToMove();
    const int dir = (us == Color::White) ? 1 : -1;        // rank direction
    const int startRank = (us == Color::White) ? 1 : 6;
    const int promoRank = (us == Color::White) ? 7 : 0;
    const int f = fileOf(from), r = rankOf(from);

    // Adds the move, expanding it into 4 moves if it reaches the last rank.
    auto add = [&](Square to, MoveFlag flag) {
        if (rankOf(to) == promoRank) {
            for (PieceType promo : PROMOTION_PIECES) out.emplace_back(from, to, flag, promo);
        } else {
            out.emplace_back(from, to, flag);
        }
    };

    // Forward one square, and two from the starting rank.
    if (onBoard(f, r + dir)) {
        Square one = makeSquare(f, r + dir);
        if (board.at(one).isEmpty()) {
            add(one, MoveFlag::Normal);
            if (r == startRank) {
                Square two = makeSquare(f, r + 2 * dir);
                if (board.at(two).isEmpty()) out.emplace_back(from, two, MoveFlag::DoublePawnPush);
            }
        }
    }
    // Diagonal captures, including en passant.
    for (int df : {-1, 1}) {
        if (!onBoard(f + df, r + dir)) continue;
        Square to = makeSquare(f + df, r + dir);
        const Piece& target = board.at(to);
        if (!target.isEmpty() && target.color != us)
            add(to, MoveFlag::Normal);
        else if (to == board.enPassantSquare())
            out.emplace_back(from, to, MoveFlag::EnPassant);
    }
}

void MoveGenerator::addPieceMoves(const Board& board, Square from, std::vector<Move>& out) {
    const Piece& piece = board.at(from);
    const Color us = piece.color;
    const int f = fileOf(from), r = rankOf(from);

    // A target is fine if it is empty or holds an enemy piece.
    auto tryStep = [&](int tf, int tr) {
        if (!onBoard(tf, tr)) return;
        const Piece& target = board.at(makeSquare(tf, tr));
        if (target.isEmpty() || target.color != us) out.emplace_back(from, makeSquare(tf, tr));
    };
    // Slide in each direction until blocked; a capture ends the ray.
    auto slide = [&](const int dirs[4][2]) {
        for (int d = 0; d < 4; ++d) {
            int tf = f + dirs[d][0], tr = r + dirs[d][1];
            while (onBoard(tf, tr)) {
                const Piece& target = board.at(makeSquare(tf, tr));
                if (target.isEmpty()) {
                    out.emplace_back(from, makeSquare(tf, tr));
                } else {
                    if (target.color != us) out.emplace_back(from, makeSquare(tf, tr));
                    break;
                }
                tf += dirs[d][0];
                tr += dirs[d][1];
            }
        }
    };

    switch (piece.type) {
        case PieceType::Knight:
            for (const auto& j : KNIGHT_JUMPS) tryStep(f + j[0], r + j[1]);
            break;
        case PieceType::King:
            for (const auto& s : KING_STEPS) tryStep(f + s[0], r + s[1]);
            break;
        case PieceType::Bishop:
            slide(BISHOP_DIRS);
            break;
        case PieceType::Rook:
            slide(ROOK_DIRS);
            break;
        case PieceType::Queen:
            slide(ROOK_DIRS);
            slide(BISHOP_DIRS);
            break;
        default:
            break;
    }
}

void MoveGenerator::addCastlingMoves(const Board& board, std::vector<Move>& out) {
    // Conditions for castling:
    //  - the right has not been lost (king and that rook never moved),
    //  - king and rook are on their home squares,
    //  - all squares between them are empty,
    //  - the king is not in check, does not pass through and does not land
    //    on an attacked square.
    const Color us = board.sideToMove();
    const Color them = opposite(us);
    const int rights = board.castlingRights();
    const Square kingHome = (us == Color::White) ? 4 : 60;  // e1 / e8
    const int kingsideRight = (us == Color::White) ? WHITE_KINGSIDE : BLACK_KINGSIDE;
    const int queensideRight = (us == Color::White) ? WHITE_QUEENSIDE : BLACK_QUEENSIDE;

    if (!(rights & (kingsideRight | queensideRight))) return;
    if (!board.at(kingHome).is(PieceType::King, us)) return;
    if (board.isSquareAttacked(kingHome, them)) return;  // cannot castle out of check

    // King side: f and g empty, f and g not attacked.
    if ((rights & kingsideRight) && board.at(kingHome + 3).is(PieceType::Rook, us) &&
        board.at(kingHome + 1).isEmpty() && board.at(kingHome + 2).isEmpty() &&
        !board.isSquareAttacked(kingHome + 1, them) && !board.isSquareAttacked(kingHome + 2, them)) {
        out.emplace_back(kingHome, kingHome + 2, MoveFlag::CastleKingside);
    }
    // Queen side: b, c and d empty, but only c and d must be safe
    // (the rook may pass an attacked b-square).
    if ((rights & queensideRight) && board.at(kingHome - 4).is(PieceType::Rook, us) &&
        board.at(kingHome - 1).isEmpty() && board.at(kingHome - 2).isEmpty() &&
        board.at(kingHome - 3).isEmpty() && !board.isSquareAttacked(kingHome - 1, them) &&
        !board.isSquareAttacked(kingHome - 2, them)) {
        out.emplace_back(kingHome, kingHome - 2, MoveFlag::CastleQueenside);
    }
}

std::vector<Move> MoveGenerator::legalMoves(Board& board) {
    std::vector<Move> pseudo;
    pseudo.reserve(64);
    pseudoLegalMoves(board, pseudo);

    std::vector<Move> legal;
    legal.reserve(pseudo.size());
    const Color us = board.sideToMove();
    for (const Move& m : pseudo) {
        board.makeMove(m);
        if (!board.inCheck(us)) legal.push_back(m);  // our king must be safe afterwards
        board.unmakeMove();
    }
    return legal;
}

std::vector<Move> MoveGenerator::legalMovesFrom(Board& board, Square from) {
    std::vector<Move> result;
    for (const Move& m : legalMoves(board))
        if (m.from == from) result.push_back(m);
    return result;
}

std::uint64_t MoveGenerator::perft(Board& board, int depth) {
    if (depth == 0) return 1;
    std::vector<Move> moves = legalMoves(board);
    if (depth == 1) return moves.size();
    std::uint64_t nodes = 0;
    for (const Move& m : moves) {
        board.makeMove(m);
        nodes += perft(board, depth - 1);
        board.unmakeMove();
    }
    return nodes;
}

}  // namespace chess
