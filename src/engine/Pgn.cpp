// Pgn.cpp
#include "Pgn.h"

#include <cctype>
#include <sstream>

namespace chess {
namespace pgn {

std::string tagValue(const PgnTags& tags, const std::string& name) {
    for (const auto& t : tags)
        if (t.first == name) return t.second;
    return "";
}

namespace {

std::string escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

}  // namespace

std::string write(const Game& game, const PgnTags& tags) {
    std::ostringstream out;
    for (const auto& t : tags)
        if (t.first != "Result" && t.first != "FEN" && t.first != "SetUp")
            out << '[' << t.first << " \"" << escape(t.second) << "\"]\n";
    out << "[Result \"" << game.result().pgnString() << "\"]\n";
    if (game.startFen() != Board(Board::START_FEN).toFen()) {
        out << "[SetUp \"1\"]\n";
        out << "[FEN \"" << game.startFen() << "\"]\n";
    }
    out << '\n';

    // Movetext, wrapped at 80 characters.
    Board start(game.startFen());
    int moveNumber = start.fullmoveNumber();
    bool whiteToMove = start.sideToMove() == Color::White;
    std::string line, text;
    auto addToken = [&](const std::string& token) {
        if (!line.empty() && line.size() + 1 + token.size() > 80) {
            text += line + '\n';
            line.clear();
        }
        if (!line.empty()) line += ' ';
        line += token;
    };
    const auto& san = game.sanHistory();
    for (std::size_t i = 0; i < san.size(); ++i) {
        if (whiteToMove)
            addToken(std::to_string(moveNumber) + ". " + san[i]);
        else if (i == 0)
            addToken(std::to_string(moveNumber) + "... " + san[i]);  // game starts with Black
        else
            addToken(san[i]);
        if (!whiteToMove) ++moveNumber;
        whiteToMove = !whiteToMove;
    }
    addToken(game.result().pgnString());
    text += line + '\n';
    out << text;
    return out.str();
}

bool read(const std::string& text, Game& game, PgnTags* tagsOut, std::string& error) {
    PgnTags tags;
    std::vector<std::string> tokens;
    std::string resultToken;

    // ---- 1. split into tags and movetext tokens ----------------------------
    std::size_t i = 0;
    const std::size_t n = text.size();
    int variationDepth = 0;
    bool sawMovetext = false;
    while (i < n) {
        char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
        } else if (c == '[' && variationDepth == 0) {
            if (sawMovetext && !tokens.empty()) break;  // start of the next game
            // [Name "Value"]
            std::size_t end = i + 1;
            std::string name, value;
            while (end < n && std::isspace(static_cast<unsigned char>(text[end]))) ++end;
            while (end < n && !std::isspace(static_cast<unsigned char>(text[end])) && text[end] != '"' && text[end] != ']')
                name += text[end++];
            while (end < n && text[end] != '"' && text[end] != ']') ++end;
            if (end < n && text[end] == '"') {
                ++end;
                while (end < n && text[end] != '"') {
                    if (text[end] == '\\' && end + 1 < n) ++end;
                    value += text[end++];
                }
                ++end;
            }
            while (end < n && text[end] != ']') ++end;
            i = end + 1;
            if (!name.empty()) tags.emplace_back(name, value);
        } else if (c == '{') {  // comment
            while (i < n && text[i] != '}') ++i;
            ++i;
        } else if (c == ';') {  // comment to end of line
            while (i < n && text[i] != '\n') ++i;
        } else if (c == '(') {
            ++variationDepth;
            ++i;
        } else if (c == ')') {
            if (variationDepth > 0) --variationDepth;
            ++i;
        } else {
            std::string token;
            while (i < n && !std::isspace(static_cast<unsigned char>(text[i])) && text[i] != '{' &&
                   text[i] != '(' && text[i] != ')' && text[i] != ';' && text[i] != '[')
                token += text[i++];
            if (variationDepth > 0 || token.empty()) continue;
            sawMovetext = true;
            if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*") {
                resultToken = token;
                if (!tokens.empty() || resultToken != "*") break;
                continue;
            }
            if (token[0] == '$') continue;  // NAG
            // Strip a move number such as "12." or "12..." (possibly glued: "12.e4").
            std::size_t k = 0;
            while (k < token.size() && std::isdigit(static_cast<unsigned char>(token[k]))) ++k;
            if (k > 0 && k < token.size() && token[k] == '.') {
                while (k < token.size() && token[k] == '.') ++k;
                token = token.substr(k);
            } else if (k == token.size()) {
                continue;  // bare number
            }
            if (!token.empty()) tokens.push_back(token);
        }
    }

    if (tags.empty() && tokens.empty()) {
        error = "No PGN game found";
        return false;
    }

    // ---- 2. replay the moves -------------------------------------------------
    Game g;
    std::string fen = tagValue(tags, "FEN");
    if (!fen.empty() && !g.newGame(fen)) {
        error = "Invalid FEN tag in PGN";
        return false;
    }
    for (std::size_t m = 0; m < tokens.size(); ++m) {
        if (!g.makeSanMove(tokens[m])) {
            error = "Illegal or unknown move \"" + tokens[m] + "\" (move " + std::to_string(m / 2 + 1) + ")";
            return false;
        }
    }

    // A finished game whose end is not visible on the board (e.g. resignation).
    std::string res = resultToken.empty() ? tagValue(tags, "Result") : resultToken;
    if (res == "1-0") g.setImportedResult(Outcome::WhiteWins);
    else if (res == "0-1") g.setImportedResult(Outcome::BlackWins);
    else if (res == "1/2-1/2") g.setImportedResult(Outcome::Draw);

    game = g;
    if (tagsOut) *tagsOut = tags;
    return true;
}

}  // namespace pgn
}  // namespace chess
