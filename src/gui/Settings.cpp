// Settings.cpp
#include "Settings.h"

#include <algorithm>
#include <fstream>
#include <string>

namespace gui {

namespace {

bool toBool(const std::string& v) { return v == "1" || v == "true" || v == "yes" || v == "on"; }

int toInt(const std::string& v, int fallback, int lo, int hi) {
    try {
        return std::max(lo, std::min(hi, std::stoi(v)));
    } catch (...) {
        return fallback;  // not a number: keep the default
    }
}

}  // namespace

bool Settings::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "show_legal_moves") showLegalMoves = toBool(value);
        else if (key == "show_coordinates") showCoordinates = toBool(value);
        else if (key == "animations") animations = toBool(value);
        else if (key == "sound") sound = toBool(value);
        else if (key == "auto_queen") autoQueen = toBool(value);
        else if (key == "auto_flip") autoFlip = toBool(value);
        else if (key == "board_theme") boardTheme = toInt(value, 0, 0, 99);
        else if (key == "mode") mode = toInt(value, 1, 0, 1);
        else if (key == "human_color") humanColor = toInt(value, 0, 0, 2);
        else if (key == "ai_level") aiLevel = toInt(value, 3, 1, 5);
        else if (key == "time_base") timeBase = toInt(value, 0, 0, 3 * 3600);
        else if (key == "time_increment") timeIncrement = toInt(value, 0, 0, 600);
        else if (key == "player_name" && !value.empty()) playerName = value.substr(0, 24);
    }
    return true;
}

bool Settings::save(const std::string& path) const {
    std::ofstream out(path);
    if (!out) return false;
    out << "# Chess settings (edit with care; delete the file to restore defaults)\n"
        << "show_legal_moves=" << showLegalMoves << '\n'
        << "show_coordinates=" << showCoordinates << '\n'
        << "animations=" << animations << '\n'
        << "sound=" << sound << '\n'
        << "auto_queen=" << autoQueen << '\n'
        << "auto_flip=" << autoFlip << '\n'
        << "board_theme=" << boardTheme << '\n'
        << "mode=" << mode << '\n'
        << "human_color=" << humanColor << '\n'
        << "ai_level=" << aiLevel << '\n'
        << "time_base=" << timeBase << '\n'
        << "time_increment=" << timeIncrement << '\n'
        << "player_name=" << playerName << '\n';
    return static_cast<bool>(out);
}

}  // namespace gui
