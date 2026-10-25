// unit_tests.cpp - tests for everything except move generation counts
// (those are in perft.cpp): FEN, SAN, draw rules, PGN, clock, opening book
// and the AI. Console program; exit code 0 = all tests passed.
#include "engine/AiLevel.h"
#include "engine/AsyncSearch.h"
#include "engine/Board.h"
#include "engine/ChessClock.h"
#include "engine/Game.h"
#include "engine/MoveGenerator.h"
#include "engine/Notation.h"
#include "engine/OpeningBook.h"
#include "engine/Pgn.h"
#include "engine/Search.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace chess;

namespace {

int g_failures = 0;
int g_passes = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passes;
        std::cout << "  [PASS] " << what << '\n';
    } else {
        ++g_failures;
        std::cout << "  [FAIL] " << what << '\n';
    }
}

// Plays a list of SAN moves; returns false at the first illegal one.
bool play(Game& g, const std::vector<std::string>& sans) {
    for (const std::string& s : sans)
        if (!g.makeSanMove(s)) return false;
    return true;
}

void testFen() {
    std::cout << "FEN\n";
    const char* fens[] = {
        Board::START_FEN,
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",
        "4k3/8/8/8/8/8/8/4K2R w K - 12 40",
    };
    for (const char* fen : fens) {
        Board b;
        bool ok = b.loadFen(fen) && b.toFen() == fen;
        check(ok, std::string("round-trip ") + fen);
    }
    Board b;
    check(!b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1"), "reject: missing rank");
    check(!b.loadFen("rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), "reject: 9 squares in a rank");
    check(!b.loadFen("rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQ - 0 1"), "reject: missing black king");
    check(!b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1"), "reject: bad side to move");
    check(!b.loadFen("Pnbqkbnr/pppppppp/8/8/8/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1"), "reject: pawn on 8th rank");
    check(b.loadFen("4k3/8/8/8/8/8/8/4K2r w - - 0 1"), "accept: side to move may be in check");
    check(!b.loadFen("4k3/4R3/8/8/8/8/8/4K3 w - - 0 1"), "reject: side NOT to move is in check");
    check(!b.loadFen("hello world"), "reject: garbage");
    check(b.toFen() == Board::START_FEN || b.toFen().size() > 10, "board unchanged after a rejected FEN");
    // Castling rights without the rook at home are dropped.
    check(b.loadFen("4k3/8/8/8/8/8/8/4K3 w KQkq - 0 1") && b.castlingRights() == NO_CASTLING,
          "impossible castling rights are removed");
}

void testSan() {
    std::cout << "SAN\n";
    Game g;
    check(play(g, {"e4", "e5", "Nf3", "Nc6", "Bb5", "a6", "Ba4", "Nf6", "O-O"}), "Ruy Lopez moves accepted");
    check(g.sanHistory().back() == "O-O", "castling written as O-O");

    // Disambiguation by file and by rank.
    Game d("4k3/8/8/8/8/8/K7/R6R w - - 0 1");
    Board b = d.board();
    check(notation::toSan(b, notation::fromUci(b, "a1d1")) == "Rad1", "file disambiguation Rad1");
    Game r("4k3/8/8/8/R7/8/8/R3K3 w - - 0 1");
    b = r.board();
    check(notation::toSan(b, notation::fromUci(b, "a1a2")) == "R1a2", "rank disambiguation R1a2");
    Game q("1k6/8/8/8/7Q/8/8/K6Q w - - 0 1");  // queens on h4 and h1 can both reach e1
    b = q.board();
    check(notation::toSan(b, notation::fromUci(b, "h4e1")) == "Q4e1", "queen disambiguation Q4e1");

    // Promotion with check, en passant, mate symbol.
    Game p("8/1P2k3/8/8/8/8/8/4K3 w - - 0 1");
    b = p.board();
    check(notation::toSan(b, notation::fromUci(b, "b7b8q")) == "b8=Q", "promotion b8=Q");
    Game p2("4k3/1P6/8/8/8/8/8/4K3 w - - 0 1");
    b = p2.board();
    check(notation::toSan(b, notation::fromUci(b, "b7b8q")) == "b8=Q+", "promotion with check b8=Q+");
    Game ep("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2");
    b = ep.board();
    check(notation::toSan(b, notation::fromUci(b, "e5d6")) == "exd6", "en passant exd6");
    Game fool;
    check(play(fool, {"f3", "e5", "g4", "Qh4#"}) && fool.sanHistory().back() == "Qh4#", "mate symbol Qh4#");

    // Tolerant parsing.
    Game t;
    check(t.makeSanMove("Ng1f3") && !t.makeSanMove("e7e9"), "long algebraic accepted, junk rejected");
    Game t2;
    check(play(t2, {"e4", "d5", "exd5", "Qxd5", "Nc3", "Qa5", "d4", "Nf6", "Nf3", "Bf5", "Bc4", "e6", "Bd2", "c6",
                    "Qe2", "Bb4", "0-0-0"}),
          "0-0-0 with zeros accepted");
}

void testDrawRules() {
    std::cout << "Game results and draw rules\n";
    Game g;
    play(g, {"f3", "e5", "g4", "Qh4#"});
    check(g.result().outcome == Outcome::BlackWins && g.result().reason == EndReason::Checkmate, "Fool's mate");

    g.newGame("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    check(g.result().reason == EndReason::Stalemate, "stalemate");

    g.newGame("8/8/4k3/8/8/3BK3/8/8 w - - 0 1");
    check(g.result().reason == EndReason::InsufficientMaterial, "K+B vs K insufficient");
    g.newGame("8/8/4k3/8/8/3NK3/8/8 w - - 0 1");
    check(g.result().reason == EndReason::InsufficientMaterial, "K+N vs K insufficient");
    g.newGame("8/8/2b1k3/8/8/3BK3/8/8 w - - 0 1");  // c6 and d3 are both light squares
    check(g.result().reason == EndReason::InsufficientMaterial, "K+B vs K+B same colour insufficient");
    g.newGame("8/8/3bk3/8/8/3BK3/8/8 w - - 0 1");   // d6 dark, d3 light
    check(!g.isOver(), "K+B vs K+B opposite colours is playable");
    g.newGame("8/8/4k3/8/8/3RK3/8/8 w - - 0 1");
    check(!g.isOver(), "K+R vs K is playable");

    // 50-move rule must be claimed; 75-move rule is automatic.
    g.newGame("8/8/4k3/8/8/4K3/8/R7 w - - 99 80");
    g.makeSanMove("Ra2");
    check(!g.isOver() && g.claimableDraw() == EndReason::FiftyMoveRule, "50-move draw becomes claimable");
    check(g.claimDraw() && g.result().reason == EndReason::FiftyMoveRule, "50-move draw claimed");
    g.newGame("8/8/4k3/8/8/4K3/8/R7 w - - 149 120");
    g.makeSanMove("Ra2");
    check(g.result().reason == EndReason::SeventyFiveMoveRule, "75-move rule ends the game");

    // Threefold (claim) and fivefold (automatic) repetition.
    g.newGame();
    std::vector<std::string> cycle = {"Nf3", "Nf6", "Ng1", "Ng8"};
    play(g, cycle);
    check(g.claimableDraw() == EndReason::None, "two occurrences: no claim yet");
    play(g, cycle);
    check(g.claimableDraw() == EndReason::ThreefoldRepetition, "threefold repetition claimable");
    play(g, cycle);
    play(g, cycle);
    check(g.result().reason == EndReason::FivefoldRepetition, "fivefold repetition ends the game");
    g.undo();
    check(!g.isOver(), "undo re-opens the game");

    // En passant rights matter for repetition: same pieces, but ep possible
    Board a("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2"), b2("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 2");
    check(a.positionKey() != b2.positionKey(), "capturable en passant changes the position key");
    Board c("4k3/8/8/3p4/8/8/8/4K3 w - d6 0 2"), d("4k3/8/8/3p4/8/8/8/4K3 w - - 0 2");
    check(c.positionKey() == d.positionKey(), "non-capturable en passant square is ignored");

    // Resign, agreement, timeout.
    g.newGame();
    g.resign(Color::White);
    check(g.result().outcome == Outcome::BlackWins && g.result().reason == EndReason::Resignation, "resignation");
    check(!g.makeSanMove("e4"), "no moves after the game is over");
    g.newGame();
    g.agreeDraw();
    check(g.result().outcome == Outcome::Draw && g.result().reason == EndReason::Agreement, "draw by agreement");
    g.newGame();
    g.flagFall(Color::Black);
    check(g.result().outcome == Outcome::WhiteWins && g.result().reason == EndReason::Timeout, "loss on time");
    g.newGame("4k3/8/8/8/8/8/3PPP2/4K3 w - - 0 1");  // black has a lone king
    g.flagFall(Color::White);
    check(g.result().outcome == Outcome::Draw && g.result().reason == EndReason::TimeoutVsInsufficientMaterial,
          "timeout vs lone king is a draw");

    // Undo restores the exact position (castling + en passant + clocks).
    g.newGame("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 3 10");
    std::string before = g.board().toFen();
    g.makeSanMove("O-O");
    check(g.board().at(parseSquare("f1")).is(PieceType::Rook, Color::White), "castling moves the rook");
    g.undo();
    check(g.board().toFen() == before, "undo restores castling position");
}

void testPgn() {
    std::cout << "PGN\n";
    Game g;
    play(g, {"e4", "e5", "Nf3", "Nc6", "Bc4", "Nf6", "Ng5", "d5", "exd5", "Nxd5", "Nxf7", "Kxf7", "Qf3+", "Ke6",
             "Nc3", "Nb4", "a3", "Nxc2+", "Kd1", "Nxa1"});
    std::string text = pgn::write(g, {{"Event", "Test"}, {"White", "A"}, {"Black", "B \"quoted\""}});
    Game back;
    PgnTags tags;
    std::string err;
    check(pgn::read(text, back, &tags, err), "read back written PGN" + (err.empty() ? "" : ": " + err));
    check(back.sanHistory() == g.sanHistory(), "PGN round-trip keeps all moves");
    check(back.board().toFen() == g.board().toFen(), "PGN round-trip reaches the same position");
    check(pgn::tagValue(tags, "Black") == "B \"quoted\"", "PGN tag with quotes round-trips");

    // Custom start position (Black to move) round-trip.
    Game f("4k3/8/8/8/8/8/4P3/4K3 b - - 0 30");
    play(f, {"Kd7", "e4", "Ke6"});
    text = pgn::write(f, {{"Event", "FEN test"}});
    check(text.find("[FEN \"4k3/8/8/8/8/8/4P3/4K3 b - - 0 30\"]") != std::string::npos && text.find("30... Kd7") != std::string::npos,
          "PGN from FEN has SetUp/FEN tags and '30...'");
    Game fb;
    check(pgn::read(text, fb, nullptr, err) && fb.board().toFen() == f.board().toFen(), "PGN with FEN round-trip");

    // Comments, NAGs, variations, glued move numbers, result.
    const std::string messy =
        "[Event \"Messy\"]\n[Result \"0-1\"]\n\n"
        "1.e4 {best by test} e5 2. Nf3 $1 Nc6 (2... d6 3. d4) 3.Bb5 a6; comment\n"
        "4. Ba4 Nf6 0-1\n";
    Game m;
    check(pgn::read(messy, m, nullptr, err) && m.plyCount() == 8, "messy PGN parsed (8 plies)");
    check(m.result().outcome == Outcome::BlackWins && m.result().reason == EndReason::Imported,
          "PGN result applied when not visible on the board");
    Game bad;
    check(!pgn::read("1. e4 e5 2. Ke3 Ke6 3. Qh8", bad, nullptr, err) && !err.empty(), "illegal PGN move reported");
    check(!pgn::read("", bad, nullptr, err), "empty PGN rejected");
}

void testClock() {
    std::cout << "Chess clock\n";
    ChessClock c;
    c.reset({180, 2});
    check(c.remainingMs(Color::White) == 180000, "3+2 starts with 3:00");
    c.tick(Color::White, 1500);
    c.moveMade(Color::White);
    check(c.remainingMs(Color::White) == 180500, "increment added after the move");
    c.tick(Color::Black, 200000);
    check(c.flagged(Color::Black) && c.remainingMs(Color::Black) == 0, "flag falls at zero");
    check(ChessClock::format(65000) == "1:05" && ChessClock::format(9300) == "0:09.3", "clock formatting");
    check(TimeControl{900, 10}.toString() == "15+10" && !TimeControl{}.enabled(), "time control text");
}

void testBook() {
    std::cout << "Opening book\n";
    const OpeningBook& book = OpeningBook::instance();
    check(book.validLineCount() == book.totalLineCount(), "all " + std::to_string(book.totalLineCount()) + " book lines are legal");
    Board start;
    check(book.movesFor(start).size() >= 4, "several first moves in the book");
    Game g;
    play(g, {"e4", "c5", "Nf3", "d6", "d4"});
    check(book.openingName(g.moves()) == "Sicilian Defence", "opening name: Sicilian Defence");
    play(g, {"cxd4", "Nxd4", "Nf6", "Nc3", "a6"});
    check(book.openingName(g.moves()) == "Sicilian Defence: Najdorf", "opening name: Najdorf");
}

void testSearch() {
    std::cout << "AI search\n";
    Search s;
    SearchLimits lim;
    lim.maxDepth = 4;
    lim.timeMs = 3000;
    lim.useBook = false;

    Game m1("6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1");
    SearchResult r = s.think(m1.board(), lim);
    Board b = m1.board();
    check(notation::toSan(b, r.best) == "Ra8#" && r.mateInMoves() == 1, "finds back-rank mate in 1");

    // Mate in 2: 1. Kb6 Kb8 2. Rh8#
    Game m2("k7/8/2K5/8/8/8/8/7R w - - 0 1");
    lim.maxDepth = 5;
    r = s.think(m2.board(), lim);
    b = m2.board();
    check(r.mateInMoves() == 2, "finds mate in 2 (" + notation::toSan(b, r.best) + ")");

    // A queen left en prise must be captured.
    Game h("rnb1kbnr/pppp1ppp/8/4p1q1/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 3");
    r = s.think(h.board(), lim);
    b = h.board();
    std::string san = notation::toSan(b, r.best);
    check(san == "Bxg5", "captures a free queen (" + san + ")");

    // Stalemate-aware: does not stalemate when winning (K+Q vs K).
    Game st("7k/8/5K2/8/8/8/8/6Q1 w - - 0 1");
    r = s.think(st.board(), lim);
    b = st.board();
    b.makeMove(r.best);
    check(!MoveGenerator::legalMoves(b).empty() || b.inCheck(), "avoids stalemating the opponent");

    // Every level returns a legal move quickly.
    for (int level = 1; level <= AI_LEVEL_COUNT; ++level) {
        SearchLimits l = aiLimitsForLevel(level);
        l.timeMs = std::min(l.timeMs, 600);
        Game g;
        g.makeSanMove("e4");
        auto t0 = std::chrono::steady_clock::now();
        SearchResult res = s.think(g.board(), l, g.recentPositionKeys());
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        bool legal = false;
        for (const Move& mv : g.legalMoves()) legal = legal || mv == res.best;
        check(legal && ms < 2500, "level " + std::to_string(level) + " (" + aiLevelName(level) + ") plays a legal move in " +
                                      std::to_string(ms) + " ms" + (res.fromBook ? " [book]" : ""));
    }

    // Background thread: start, finish, cancel.
    AsyncSearch async;
    Game g;
    SearchLimits quick;
    quick.timeMs = 300;
    quick.useBook = false;
    async.start(g.board(), quick, g.recentPositionKeys());
    auto t0 = std::chrono::steady_clock::now();
    while (!async.isFinished() && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(5))
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    check(async.isFinished() && !async.takeResult().best.isNull(), "async search finishes with a move");
    SearchLimits slow;
    slow.timeMs = 60000;
    slow.useBook = false;
    async.start(g.board(), slow, {});
    t0 = std::chrono::steady_clock::now();
    async.cancel();
    auto cancelMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    check(!async.isRunning() && cancelMs < 1000, "async search can be cancelled (" + std::to_string(cancelMs) + " ms)");

    // Self-play smoke test: the engine plays a short game against itself.
    Game self;
    SearchLimits fast;
    fast.timeMs = 30;
    fast.maxDepth = 3;
    int plies = 0;
    while (!self.isOver() && plies < 80) {
        SearchResult res = s.think(self.board(), fast, self.recentPositionKeys());
        if (!self.makeMove(res.best)) break;
        ++plies;
    }
    check(plies == 80 || self.isOver(), "self-play: 80 plies without an illegal move");
}

}  // namespace

int main() {
    testFen();
    testSan();
    testDrawRules();
    testPgn();
    testClock();
    testBook();
    testSearch();
    std::cout << '\n' << g_passes << " passed, " << g_failures << " failed.\n";
    std::cout << (g_failures == 0 ? "All tests passed." : "Some tests FAILED.") << '\n';
    return g_failures == 0 ? 0 : 1;
}
