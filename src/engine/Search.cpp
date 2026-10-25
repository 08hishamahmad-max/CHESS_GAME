// Search.cpp
#include "Search.h"

#include "MoveGenerator.h"
#include "OpeningBook.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace chess {

// ===========================================================================
// Evaluation
// ===========================================================================
namespace {

// Material values in centipawns, indexed by PieceType (None, P, N, B, R, Q, K).
const int VALUE_MG[7] = {0, 100, 320, 330, 500, 900, 0};
const int VALUE_EG[7] = {0, 120, 300, 320, 520, 930, 0};
// Contribution of each piece type to the "game phase" (24 = all pieces on board).
const int PHASE_WEIGHT[7] = {0, 0, 1, 1, 2, 4, 0};

// Piece-square tables ("Simplified Evaluation Function", T. Michniewski).
// Written as you look at the board from White's side: first row = rank 8.
const int PST_PAWN[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0};
const int PST_KNIGHT[64] = {
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50};
const int PST_BISHOP[64] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -20,-10,-10,-10,-10,-10,-10,-20};
const int PST_ROOK[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0};
const int PST_QUEEN[64] = {
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20};
const int PST_KING_MG[64] = {  // middlegame: hide behind the pawns
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20};
const int PST_KING_EG[64] = {  // endgame: centralise the king
   -50,-40,-30,-20,-20,-30,-40,-50,
   -30,-20,-10,  0,  0,-10,-20,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-30,  0,  0,  0,  0,-30,-30,
   -50,-30,-30,-30,-30,-30,-30,-50};

// Bonus for a passed pawn by its rank seen from its own side (0..7).
const int PASSED_MG[8] = {0, 5, 10, 15, 25, 40, 60, 0};
const int PASSED_EG[8] = {0, 10, 20, 35, 60, 100, 150, 0};

const int* pstFor(PieceType t) {
    switch (t) {
        case PieceType::Pawn:   return PST_PAWN;
        case PieceType::Knight: return PST_KNIGHT;
        case PieceType::Bishop: return PST_BISHOP;
        case PieceType::Rook:   return PST_ROOK;
        case PieceType::Queen:  return PST_QUEEN;
        default:                return nullptr;
    }
}

// Distance of a square from the centre (0 in the middle, 6 in a corner).
int centreDistance(Square sq) {
    int f = fileOf(sq), r = rankOf(sq);
    return std::max(3 - f, f - 4) + std::max(3 - r, r - 4);
}

int kingDistance(Square a, Square b) { return std::abs(fileOf(a) - fileOf(b)) + std::abs(rankOf(a) - rankOf(b)); }

}  // namespace

int Search::evaluate(const Board& board) {
    int mg[2] = {0, 0}, eg[2] = {0, 0};
    int phase = 0;
    int bishops[2] = {0, 0};
    int nonPawn[2] = {0, 0}, pawnCount[2] = {0, 0};
    int pawnsOnFile[2][8] = {{0}};

    // Pass 1: material and piece-square tables.
    for (Square sq = 0; sq < 64; ++sq) {
        const Piece& p = board.at(sq);
        if (p.isEmpty()) continue;
        const int c = static_cast<int>(p.color), t = static_cast<int>(p.type);
        // The tables are written for White with rank 8 first, so White's
        // index is "flipped" and Black's is the plain square number mirrored.
        const int idx = (p.color == Color::White) ? (7 - rankOf(sq)) * 8 + fileOf(sq) : rankOf(sq) * 8 + fileOf(sq);
        phase += PHASE_WEIGHT[t];
        if (p.type == PieceType::King) {
            mg[c] += PST_KING_MG[idx];
            eg[c] += PST_KING_EG[idx];
            continue;
        }
        const int pst = pstFor(p.type)[idx];
        mg[c] += VALUE_MG[t] + pst;
        eg[c] += VALUE_EG[t] + pst;
        if (p.type == PieceType::Pawn) {
            ++pawnCount[c];
            ++pawnsOnFile[c][fileOf(sq)];
        } else {
            nonPawn[c] += VALUE_MG[t];
        }
        if (p.type == PieceType::Bishop) ++bishops[c];
    }

    // Pass 2: pawn structure and rooks on open files.
    for (Square sq = 0; sq < 64; ++sq) {
        const Piece& p = board.at(sq);
        if (p.isEmpty()) continue;
        const int c = static_cast<int>(p.color), them = 1 - c;
        const int f = fileOf(sq), r = rankOf(sq);
        if (p.type == PieceType::Pawn) {
            // Passed pawn: no enemy pawn in front of it on its own or the adjacent files.
            bool passed = true;
            for (int df = -1; df <= 1 && passed; ++df) {
                if (f + df < 0 || f + df > 7) continue;
                for (int rr = r + (c == 0 ? 1 : -1); rr >= 0 && rr < 8; rr += (c == 0 ? 1 : -1))
                    if (board.at(makeSquare(f + df, rr)).is(PieceType::Pawn, static_cast<Color>(them))) {
                        passed = false;
                        break;
                    }
            }
            if (passed) {
                int relRank = (c == 0) ? r : 7 - r;
                mg[c] += PASSED_MG[relRank];
                eg[c] += PASSED_EG[relRank];
            }
            // Isolated pawn: no friendly pawns on adjacent files.
            bool left = f > 0 && pawnsOnFile[c][f - 1] > 0, right = f < 7 && pawnsOnFile[c][f + 1] > 0;
            if (!left && !right) {
                mg[c] -= 12;
                eg[c] -= 15;
            }
        } else if (p.type == PieceType::Rook) {
            if (pawnsOnFile[c][f] == 0) {
                int bonus = pawnsOnFile[them][f] == 0 ? 20 : 10;  // open : half-open file
                mg[c] += bonus;
                eg[c] += bonus / 2;
            }
        }
    }
    for (int c = 0; c < 2; ++c) {
        for (int f = 0; f < 8; ++f)
            if (pawnsOnFile[c][f] > 1) {  // doubled pawns
                mg[c] -= 10 * (pawnsOnFile[c][f] - 1);
                eg[c] -= 20 * (pawnsOnFile[c][f] - 1);
            }
        if (bishops[c] >= 2) {
            mg[c] += 30;
            eg[c] += 50;
        }
    }

    // Blend middlegame and endgame scores by the game phase.
    phase = std::min(phase, 24);
    int score = ((mg[0] - mg[1]) * phase + (eg[0] - eg[1]) * (24 - phase)) / 24;

    // Mop-up: with a decisive material advantage against a lone king, drive
    // the enemy king to the edge and bring our king closer (needed to mate).
    for (int c = 0; c < 2; ++c) {
        int them = 1 - c;
        if (pawnCount[them] == 0 && nonPawn[them] == 0 && nonPawn[c] >= 500) {
            Square loserKing = board.kingSquare(static_cast<Color>(them));
            Square winnerKing = board.kingSquare(static_cast<Color>(c));
            int bonus = 10 * centreDistance(loserKing) + 4 * (14 - kingDistance(loserKing, winnerKing));
            score += (c == 0) ? bonus : -bonus;
        }
    }

    return board.sideToMove() == Color::White ? score : -score;
}

// ===========================================================================
// Search
// ===========================================================================
namespace {

constexpr std::size_t TT_SIZE = 1u << 19;  // 524288 entries (~20 MB)

bool isCapture(const Board& board, const Move& m) {
    return !board.at(m.to).isEmpty() || m.flag == MoveFlag::EnPassant;
}

// Mate scores are stored relative to the current node in the hash table so
// that they stay correct when the same position is reached at another ply.
int scoreToTT(int score, int ply) {
    if (score > Search::MATE - 1000) return score + ply;
    if (score < -Search::MATE + 1000) return score - ply;
    return score;
}
int scoreFromTT(int score, int ply) {
    if (score > Search::MATE - 1000) return score - ply;
    if (score < -Search::MATE + 1000) return score + ply;
    return score;
}

}  // namespace

bool SearchResult::isMateScore() const { return std::abs(score) > Search::MATE - 1000; }

int SearchResult::mateInMoves() const {
    if (!isMateScore()) return 0;
    int plies = Search::MATE - std::abs(score);
    int moves = (plies + 1) / 2;
    return score > 0 ? moves : -moves;
}

Search::Search() : table_(TT_SIZE), rng_(std::random_device{}()) {
    std::memset(history_, 0, sizeof history_);
}

void Search::clearTranspositionTable() { std::fill(table_.begin(), table_.end(), TTEntry{}); }

void Search::checkTime() {
    if (stopFlag_ && stopFlag_->load()) {
        stopped_ = true;
        return;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime_);
    if (timeLimitMs_ > 0 && elapsed.count() >= timeLimitMs_) stopped_ = true;
}

bool Search::isRepetition(std::uint64_t key, int halfmoveClock) const {
    // Look back through positions with the same side to move (every 2nd ply),
    // but not past the last irreversible move.
    const int n = static_cast<int>(pathKeys_.size());
    for (int i = n - 3, back = 2; i >= 0 && back <= halfmoveClock; i -= 2, back += 2)
        if (pathKeys_[i] == key) return true;
    return false;
}

void Search::orderMoves(const Board& board, std::vector<Move>& moves, const Move& ttMove, int ply) const {
    // Higher score = searched earlier. Good ordering = many more cut-offs.
    auto score = [&](const Move& m) {
        if (m == ttMove) return 10000000;  // best move from the hash table
        if (isCapture(board, m)) {
            // MVV-LVA: Most Valuable Victim, Least Valuable Attacker.
            int victim = m.flag == MoveFlag::EnPassant ? 100 : VALUE_MG[static_cast<int>(board.at(m.to).type)];
            int attacker = VALUE_MG[static_cast<int>(board.at(m.from).type)];
            return 1000000 + victim * 10 - attacker / 10 + (m.isPromotion() ? 9000 : 0);
        }
        if (m.isPromotion()) return 900000 + VALUE_MG[static_cast<int>(m.promotion)];
        if (ply < MAX_PLY) {
            if (m == killers_[ply][0]) return 800000;  // quiet moves that caused cut-offs
            if (m == killers_[ply][1]) return 700000;
        }
        return std::min(history_[m.from][m.to], 600000);
    };
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    for (const Move& m : moves) scored.emplace_back(score(m), m);
    std::stable_sort(scored.begin(), scored.end(),
                     [](const std::pair<int, Move>& a, const std::pair<int, Move>& b) { return a.first > b.first; });
    for (std::size_t i = 0; i < moves.size(); ++i) moves[i] = scored[i].second;
}

SearchResult Search::think(const Board& position, const SearchLimits& limits,
                           const std::vector<std::uint64_t>& history, const std::atomic<bool>* stop) {
    SearchResult result;
    Board board = position;  // work on a private copy
    std::vector<Move> rootMoves = MoveGenerator::legalMoves(board);
    if (rootMoves.empty()) return result;

    // 1. Opening book
    if (limits.useBook) {
        Move book = OpeningBook::instance().probe(board, rng_);
        if (!book.isNull()) {
            result.best = book;
            result.fromBook = true;
            return result;
        }
    }

    // 2. Prepare the search
    startTime_ = std::chrono::steady_clock::now();
    timeLimitMs_ = std::max(1, limits.timeMs);
    stopFlag_ = stop;
    stopped_ = false;
    nodes_ = 0;
    std::memset(killers_, 0, sizeof killers_);
    for (auto& row : history_)
        for (int& h : row) h /= 8;  // age the history scores from the previous search
    pathKeys_ = history;
    if (pathKeys_.empty() || pathKeys_.back() != board.positionKey()) pathKeys_.push_back(board.positionKey());

    // Shuffle so that equally good moves are not always played in the same order.
    std::shuffle(rootMoves.begin(), rootMoves.end(), rng_);
    result.best = rootMoves.front();
    if (rootMoves.size() == 1) {  // only one legal move: no need to think
        result.depth = 1;
        return result;
    }

    // 3. Iterative deepening: depth 1, 2, 3, ... The best move of the previous
    //    iteration is searched first, which makes the next one much faster.
    Move previousBest;
    int completedDepth = 0;
    for (int depth = 1; depth <= limits.maxDepth; ++depth) {
        orderMoves(board, rootMoves, previousBest, 0);
        int alpha = -INF;
        const int beta = INF;
        Move bestThisIteration;
        int bestScore = -INF;
        for (const Move& m : rootMoves) {
            board.makeMove(m);
            pathKeys_.push_back(board.positionKey());
            int score = -negamax(board, depth - 1, -beta, -alpha, 1);
            pathKeys_.pop_back();
            board.unmakeMove();
            if (stopped_) break;
            if (score > bestScore) {
                bestScore = score;
                bestThisIteration = m;
            }
            if (score > alpha) alpha = score;
        }
        if (stopped_ && depth > 1) break;  // unfinished iteration: keep the previous result
        if (bestThisIteration.isNull()) break;

        previousBest = bestThisIteration;
        completedDepth = depth;
        result.best = bestThisIteration;
        result.score = bestScore;
        result.depth = depth;
        if (stopped_) break;
        if (std::abs(bestScore) > MATE - 1000) break;  // found a forced mate

        // Not enough time left to finish another (longer) iteration.
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime_);
        if (elapsed.count() * 2 > timeLimitMs_) break;
    }

    // 4. Weaker levels: choose randomly among moves that are "good enough".
    //    Every root move gets an exact score (full window), then one within
    //    the margin is picked.
    if (limits.randomMarginCp > 0 && completedDepth >= 1) {
        stopped_ = false;
        timeLimitMs_ = std::max(timeLimitMs_, 2000);  // safety net
        startTime_ = std::chrono::steady_clock::now();
        std::vector<std::pair<int, Move>> scored;
        int best = -INF;
        for (const Move& m : rootMoves) {
            board.makeMove(m);
            pathKeys_.push_back(board.positionKey());
            int score = -negamax(board, completedDepth - 1, -INF, INF, 1);
            pathKeys_.pop_back();
            board.unmakeMove();
            if (stopped_) break;
            scored.emplace_back(score, m);
            best = std::max(best, score);
        }
        if (!stopped_ && !scored.empty()) {
            std::vector<std::pair<int, Move>> candidates;
            for (const auto& s : scored)
                if (s.first >= best - limits.randomMarginCp) candidates.push_back(s);
            std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
            const auto& choice = candidates[pick(rng_)];
            result.best = choice.second;
            result.score = choice.first;
        }
    }

    result.nodes = nodes_;
    return result;
}

int Search::negamax(Board& board, int depth, int alpha, int beta, int ply) {
    if ((++nodes_ & 2047) == 0) checkTime();
    if (stopped_) return 0;

    const std::uint64_t key = board.positionKey();
    if (isRepetition(key, board.halfmoveClock()) || board.halfmoveClock() >= 100) return 0;  // draw
    if (ply >= MAX_PLY - 1) return evaluate(board);

    const bool inCheck = board.inCheck();
    if (inCheck) ++depth;  // check extension: never stop searching while in check
    if (depth <= 0) return quiescence(board, alpha, beta, ply);

    // Transposition table lookup.
    TTEntry& entry = table_[key & (TT_SIZE - 1)];
    Move ttMove;
    if (entry.key == key) {
        ttMove = entry.move;
        if (entry.depth >= depth) {
            int s = scoreFromTT(entry.score, ply);
            if (entry.bound == Bound::Exact) return s;
            if (entry.bound == Bound::Lower && s >= beta) return s;
            if (entry.bound == Bound::Upper && s <= alpha) return s;
        }
    }

    std::vector<Move> moves = MoveGenerator::legalMoves(board);
    if (moves.empty()) return inCheck ? -MATE + ply : 0;  // checkmate or stalemate
    orderMoves(board, moves, ttMove, ply);

    const int alphaOriginal = alpha;
    int bestScore = -INF;
    Move bestMove;
    for (const Move& m : moves) {
        board.makeMove(m);
        pathKeys_.push_back(board.positionKey());
        int score = -negamax(board, depth - 1, -beta, -alpha, ply + 1);
        pathKeys_.pop_back();
        board.unmakeMove();
        if (stopped_) return 0;

        if (score > bestScore) {
            bestScore = score;
            bestMove = m;
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {
            // Beta cut-off. Remember quiet moves that caused it.
            if (!isCapture(board, m) && !m.isPromotion()) {
                if (killers_[ply][0] != m) {
                    killers_[ply][1] = killers_[ply][0];
                    killers_[ply][0] = m;
                }
                history_[m.from][m.to] += depth * depth;
            }
            break;
        }
    }

    // Store the result in the transposition table (always replace).
    entry.key = key;
    entry.move = bestMove;
    entry.score = scoreToTT(bestScore, ply);
    entry.depth = static_cast<std::int16_t>(depth);
    entry.bound = bestScore <= alphaOriginal ? Bound::Upper : (bestScore >= beta ? Bound::Lower : Bound::Exact);
    return bestScore;
}

int Search::quiescence(Board& board, int alpha, int beta, int ply) {
    if ((++nodes_ & 2047) == 0) checkTime();
    if (stopped_) return 0;

    // "Stand pat": we may assume the side to move can at least keep the
    // current evaluation by making a quiet move.
    const int standPat = evaluate(board);
    if (standPat >= beta) return standPat;
    if (standPat > alpha) alpha = standPat;
    if (ply >= MAX_PLY - 1) return standPat;

    std::vector<Move> all = MoveGenerator::legalMoves(board);
    std::vector<Move> tactical;
    for (const Move& m : all)
        if (isCapture(board, m) || m.promotion == PieceType::Queen) tactical.push_back(m);
    orderMoves(board, tactical, Move{}, MAX_PLY);

    int best = standPat;
    for (const Move& m : tactical) {
        board.makeMove(m);
        int score = -quiescence(board, -beta, -alpha, ply + 1);
        board.unmakeMove();
        if (stopped_) return 0;
        if (score > best) best = score;
        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }
    return best;
}

}  // namespace chess
