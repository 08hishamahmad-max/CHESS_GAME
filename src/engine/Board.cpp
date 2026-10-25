// Board.cpp - position representation, FEN, attacks, make/unmake.
#include "Board.h"

#include <cctype>
#include <sstream>

namespace chess {

const char* const Board::START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

namespace {

// Knight and king jumps as (file, rank) offsets.
const int KNIGHT_JUMPS[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
const int KING_STEPS[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
const int ROOK_DIRS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
const int BISHOP_DIRS[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

// Castling rights that survive a move touching a given square. Moving a king
// or a rook (or capturing a rook on its home square) removes the right.
int castlingMaskFor(Square sq) {
    switch (sq) {
        case 0:  return ALL_CASTLING & ~WHITE_QUEENSIDE;                     // a1
        case 7:  return ALL_CASTLING & ~WHITE_KINGSIDE;                      // h1
        case 4:  return ALL_CASTLING & ~(WHITE_KINGSIDE | WHITE_QUEENSIDE);  // e1
        case 56: return ALL_CASTLING & ~BLACK_QUEENSIDE;                     // a8
        case 63: return ALL_CASTLING & ~BLACK_KINGSIDE;                      // h8
        case 60: return ALL_CASTLING & ~(BLACK_KINGSIDE | BLACK_QUEENSIDE);  // e8
        default: return ALL_CASTLING;
    }
}

// ---- Zobrist hashing ------------------------------------------------------
// Each (piece, square) pair, the side to move, every castling state and every
// en passant file gets a fixed random 64-bit number. The key of a position is
// the XOR of the numbers of everything present in it.
struct ZobristTable {
    std::uint64_t pieces[2][7][64];
    std::uint64_t blackToMove;
    std::uint64_t castling[16];
    std::uint64_t epFile[8];

    ZobristTable() {
        std::uint64_t state = 0x9E3779B97F4A7C15ULL;  // fixed seed -> reproducible
        auto next = [&state]() {                       // splitmix64 generator
            std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        };
        for (auto& color : pieces)
            for (auto& type : color)
                for (auto& sq : type) sq = next();
        blackToMove = next();
        for (auto& c : castling) c = next();
        for (auto& f : epFile) f = next();
    }
};

const ZobristTable& zobrist() {
    static const ZobristTable table;
    return table;
}

}  // namespace

Board::Board() { loadFen(START_FEN); }

Board::Board(const std::string& fen) {
    if (!loadFen(fen)) loadFen(START_FEN);
}

void Board::clear() {
    squares_.fill(Piece{});
    sideToMove_ = Color::White;
    castling_ = NO_CASTLING;
    epSquare_ = NO_SQUARE;
    halfmoveClock_ = 0;
    fullmoveNumber_ = 1;
    kingSquare_ = {{NO_SQUARE, NO_SQUARE}};
    history_.clear();
}

void Board::setPiece(Square sq, Piece p) {
    squares_[sq] = p;
    if (p.type == PieceType::King) kingSquare_[static_cast<int>(p.color)] = sq;
}

// ---------------------------------------------------------------------------
// FEN
// ---------------------------------------------------------------------------
bool Board::loadFen(const std::string& fen) {
    // Parse into a temporary board first so that a bad FEN leaves *this unchanged.
    Board b(NoInit{});
    b.clear();
    std::istringstream in(fen);
    std::string placement, side, castling, ep;
    int halfmove = 0, fullmove = 1;
    if (!(in >> placement >> side)) return false;
    if (!(in >> castling)) castling = "-";
    if (!(in >> ep)) ep = "-";
    if (!(in >> halfmove)) halfmove = 0;
    if (!(in >> fullmove)) fullmove = 1;
    if (halfmove < 0 || fullmove < 1) return false;

    // 1. piece placement, starting at a8 and going rank by rank downwards
    int file = 0, rank = 7;
    int kings[2] = {0, 0};
    for (char ch : placement) {
        if (ch == '/') {
            if (file != 8 || rank == 0) return false;  // every rank must have 8 squares
            file = 0;
            --rank;
        } else if (ch >= '1' && ch <= '8') {
            file += ch - '0';
            if (file > 8) return false;
        } else {
            Color color = std::isupper(static_cast<unsigned char>(ch)) ? Color::White : Color::Black;
            PieceType type;
            switch (std::tolower(static_cast<unsigned char>(ch))) {
                case 'p': type = PieceType::Pawn; break;
                case 'n': type = PieceType::Knight; break;
                case 'b': type = PieceType::Bishop; break;
                case 'r': type = PieceType::Rook; break;
                case 'q': type = PieceType::Queen; break;
                case 'k': type = PieceType::King; break;
                default: return false;
            }
            if (!onBoard(file, rank)) return false;
            if (type == PieceType::Pawn && (rank == 0 || rank == 7)) return false;  // pawns never on rank 1/8
            if (type == PieceType::King) ++kings[static_cast<int>(color)];
            b.setPiece(makeSquare(file, rank), Piece(type, color));
            ++file;
        }
    }
    if (rank != 0 || file != 8) return false;
    if (kings[0] != 1 || kings[1] != 1) return false;  // exactly one king each

    // 2. side to move
    if (side != "w" && side != "b") return false;
    b.sideToMove_ = (side == "b") ? Color::Black : Color::White;
    // The side that just moved cannot still be in check.
    if (b.inCheck(opposite(b.sideToMove_))) return false;

    // 3. castling rights (rights whose king/rook are not at home are dropped)
    b.castling_ = NO_CASTLING;
    for (char ch : castling) {
        if (ch == 'K') b.castling_ |= WHITE_KINGSIDE;
        else if (ch == 'Q') b.castling_ |= WHITE_QUEENSIDE;
        else if (ch == 'k') b.castling_ |= BLACK_KINGSIDE;
        else if (ch == 'q') b.castling_ |= BLACK_QUEENSIDE;
        else if (ch != '-') return false;
    }
    if (!b.at(4).is(PieceType::King, Color::White)) b.castling_ &= ~(WHITE_KINGSIDE | WHITE_QUEENSIDE);
    if (!b.at(7).is(PieceType::Rook, Color::White)) b.castling_ &= ~WHITE_KINGSIDE;
    if (!b.at(0).is(PieceType::Rook, Color::White)) b.castling_ &= ~WHITE_QUEENSIDE;
    if (!b.at(60).is(PieceType::King, Color::Black)) b.castling_ &= ~(BLACK_KINGSIDE | BLACK_QUEENSIDE);
    if (!b.at(63).is(PieceType::Rook, Color::Black)) b.castling_ &= ~BLACK_KINGSIDE;
    if (!b.at(56).is(PieceType::Rook, Color::Black)) b.castling_ &= ~BLACK_QUEENSIDE;

    // 4. en passant square: kept only if it is consistent with a pawn that
    //    just made a double step (otherwise ignored).
    b.epSquare_ = NO_SQUARE;
    if (ep != "-") {
        Square sq = parseSquare(ep);
        if (sq == NO_SQUARE) return false;
        const bool whiteToMove = b.sideToMove_ == Color::White;
        const int expectedRank = whiteToMove ? 5 : 2;
        const Square pawnSq = whiteToMove ? sq - 8 : sq + 8;
        if (rankOf(sq) == expectedRank && b.at(sq).isEmpty() &&
            b.at(pawnSq).is(PieceType::Pawn, opposite(b.sideToMove_)))
            b.epSquare_ = sq;
    }

    // 5-6. clocks
    b.halfmoveClock_ = halfmove;
    b.fullmoveNumber_ = fullmove;
    *this = b;
    return true;
}

std::string Board::toFen() const {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece& p = squares_[makeSquare(file, rank)];
            if (p.isEmpty()) {
                ++empty;
                continue;
            }
            if (empty) fen += char('0' + empty);
            empty = 0;
            char letter = pieceLetter(p.type);
            fen += (p.color == Color::White) ? letter : char(std::tolower(letter));
        }
        if (empty) fen += char('0' + empty);
        if (rank > 0) fen += '/';
    }
    fen += (sideToMove_ == Color::White) ? " w " : " b ";
    std::string c;
    if (castling_ & WHITE_KINGSIDE) c += 'K';
    if (castling_ & WHITE_QUEENSIDE) c += 'Q';
    if (castling_ & BLACK_KINGSIDE) c += 'k';
    if (castling_ & BLACK_QUEENSIDE) c += 'q';
    fen += c.empty() ? "-" : c;
    fen += ' ' + squareName(epSquare_);
    fen += ' ' + std::to_string(halfmoveClock_) + ' ' + std::to_string(fullmoveNumber_);
    return fen;
}

// ---------------------------------------------------------------------------
// Attacks
// ---------------------------------------------------------------------------
bool Board::isSquareAttacked(Square sq, Color by) const {
    const int f = fileOf(sq), r = rankOf(sq);

    // Pawns: a white pawn attacks diagonally upwards, so it attacks `sq`
    // from one rank *below*; a black pawn from one rank above.
    const int pawnRank = (by == Color::White) ? r - 1 : r + 1;
    for (int df : {-1, 1}) {
        if (onBoard(f + df, pawnRank) && squares_[makeSquare(f + df, pawnRank)].is(PieceType::Pawn, by))
            return true;
    }
    // Knights
    for (const auto& j : KNIGHT_JUMPS) {
        if (onBoard(f + j[0], r + j[1]) && squares_[makeSquare(f + j[0], r + j[1])].is(PieceType::Knight, by))
            return true;
    }
    // King
    for (const auto& s : KING_STEPS) {
        if (onBoard(f + s[0], r + s[1]) && squares_[makeSquare(f + s[0], r + s[1])].is(PieceType::King, by))
            return true;
    }
    // Sliding pieces: walk each ray until the first piece.
    auto rayHits = [&](const int dirs[4][2], PieceType slider) {
        for (int d = 0; d < 4; ++d) {
            int cf = f + dirs[d][0], cr = r + dirs[d][1];
            while (onBoard(cf, cr)) {
                const Piece& p = squares_[makeSquare(cf, cr)];
                if (!p.isEmpty()) {
                    if (p.color == by && (p.type == slider || p.type == PieceType::Queen)) return true;
                    break;
                }
                cf += dirs[d][0];
                cr += dirs[d][1];
            }
        }
        return false;
    };
    return rayHits(ROOK_DIRS, PieceType::Rook) || rayHits(BISHOP_DIRS, PieceType::Bishop);
}

bool Board::inCheck(Color c) const {
    Square k = kingSquare(c);
    return k != NO_SQUARE && isSquareAttacked(k, opposite(c));
}

// ---------------------------------------------------------------------------
// Zobrist key
// ---------------------------------------------------------------------------
std::uint64_t Board::positionKey() const {
    const ZobristTable& z = zobrist();
    std::uint64_t key = 0;
    for (Square sq = 0; sq < 64; ++sq) {
        const Piece& p = squares_[sq];
        if (!p.isEmpty()) key ^= z.pieces[static_cast<int>(p.color)][static_cast<int>(p.type)][sq];
    }
    if (sideToMove_ == Color::Black) key ^= z.blackToMove;
    key ^= z.castling[castling_];

    // FIDE: positions only differ by en passant if the capture is possible,
    // so include the ep square only when an enemy pawn stands next to it.
    if (epSquare_ != NO_SQUARE) {
        int f = fileOf(epSquare_);
        int pawnRank = (sideToMove_ == Color::White) ? 4 : 3;  // rank of the capturing pawn
        for (int df : {-1, 1}) {
            if (onBoard(f + df, pawnRank) &&
                squares_[makeSquare(f + df, pawnRank)].is(PieceType::Pawn, sideToMove_)) {
                key ^= z.epFile[f];
                break;
            }
        }
    }
    return key;
}

// ---------------------------------------------------------------------------
// Make / unmake
// ---------------------------------------------------------------------------
void Board::makeMove(const Move& m) {
    const Piece moving = squares_[m.from];
    const Color us = sideToMove_;

    UndoInfo undo{m, squares_[m.to], castling_, epSquare_, halfmoveClock_, fullmoveNumber_};

    // Captured piece for en passant is not on the target square.
    Square epVictim = NO_SQUARE;
    if (m.flag == MoveFlag::EnPassant) {
        epVictim = (us == Color::White) ? m.to - 8 : m.to + 8;
        undo.captured = squares_[epVictim];
        squares_[epVictim] = Piece{};
    }
    history_.push_back(undo);

    // 50-move rule counter: reset on pawn moves and captures.
    if (moving.type == PieceType::Pawn || !undo.captured.isEmpty())
        halfmoveClock_ = 0;
    else
        ++halfmoveClock_;

    // Move the piece (promoting if needed).
    squares_[m.from] = Piece{};
    setPiece(m.to, m.isPromotion() ? Piece(m.promotion, us) : moving);

    // Castling also moves the rook.
    if (m.flag == MoveFlag::CastleKingside) {
        Square rookFrom = m.to + 1, rookTo = m.to - 1;  // h-file -> f-file
        squares_[rookTo] = squares_[rookFrom];
        squares_[rookFrom] = Piece{};
    } else if (m.flag == MoveFlag::CastleQueenside) {
        Square rookFrom = m.to - 2, rookTo = m.to + 1;  // a-file -> d-file
        squares_[rookTo] = squares_[rookFrom];
        squares_[rookFrom] = Piece{};
    }

    // New en passant square: the square the pawn jumped over.
    epSquare_ = (m.flag == MoveFlag::DoublePawnPush) ? (m.from + m.to) / 2 : NO_SQUARE;

    castling_ &= castlingMaskFor(m.from) & castlingMaskFor(m.to);

    if (us == Color::Black) ++fullmoveNumber_;
    sideToMove_ = opposite(us);
}

void Board::unmakeMove() {
    if (history_.empty()) return;
    const UndoInfo undo = history_.back();
    history_.pop_back();
    const Move& m = undo.move;

    sideToMove_ = opposite(sideToMove_);
    const Color us = sideToMove_;

    // Put the moving piece back (a promoted piece becomes a pawn again).
    Piece moved = squares_[m.to];
    if (m.isPromotion()) moved = Piece(PieceType::Pawn, us);
    setPiece(m.from, moved);

    if (m.flag == MoveFlag::EnPassant) {
        squares_[m.to] = Piece{};
        squares_[(us == Color::White) ? m.to - 8 : m.to + 8] = undo.captured;
    } else {
        squares_[m.to] = undo.captured;
    }

    if (m.flag == MoveFlag::CastleKingside) {
        squares_[m.to + 1] = squares_[m.to - 1];
        squares_[m.to - 1] = Piece{};
    } else if (m.flag == MoveFlag::CastleQueenside) {
        squares_[m.to - 2] = squares_[m.to + 1];
        squares_[m.to + 1] = Piece{};
    }

    castling_ = undo.castling;
    epSquare_ = undo.epSquare;
    halfmoveClock_ = undo.halfmoveClock;
    fullmoveNumber_ = undo.fullmoveNumber;
}

}  // namespace chess
