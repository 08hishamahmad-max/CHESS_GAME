// Move.cpp
#include "Move.h"

#include <cctype>

namespace chess {

std::string Move::toUci() const {
    if (isNull()) return "0000";
    std::string s = squareName(from) + squareName(to);
    if (isPromotion()) s += char(std::tolower(pieceLetter(promotion)));
    return s;
}

}  // namespace chess
