// AiLevel.h - the five computer difficulty levels.
#pragma once

#include "Search.h"

#include <algorithm>
#include <string>

namespace chess {

constexpr int AI_LEVEL_COUNT = 5;

inline std::string aiLevelName(int level) {
    static const char* const names[AI_LEVEL_COUNT] = {"Beginner", "Casual", "Club", "Advanced", "Strong"};
    return names[std::max(1, std::min(level, AI_LEVEL_COUNT)) - 1];
}

// Search limits for a level (1..5). With a running clock the time per move is
// also limited by the remaining time: about 1/25 of it plus most of the increment.
inline SearchLimits aiLimitsForLevel(int level, long long clockRemainingMs = 0, int incrementMs = 0) {
    SearchLimits l;
    switch (std::max(1, std::min(level, AI_LEVEL_COUNT))) {
        case 1: l.maxDepth = 1;  l.timeMs = 300;  l.randomMarginCp = 220; l.useBook = false; break;
        case 2: l.maxDepth = 2;  l.timeMs = 500;  l.randomMarginCp = 90;  break;
        case 3: l.maxDepth = 3;  l.timeMs = 800;  l.randomMarginCp = 25;  break;
        case 4: l.maxDepth = 6;  l.timeMs = 1500; l.randomMarginCp = 0;   break;
        default: l.maxDepth = 64; l.timeMs = 4000; l.randomMarginCp = 0;  break;
    }
    if (clockRemainingMs > 0) {
        long long budget = clockRemainingMs / 25 + incrementMs * 3 / 4;
        budget = std::min(budget, clockRemainingMs / 2);  // never risk the flag
        l.timeMs = static_cast<int>(std::max(30LL, std::min<long long>(l.timeMs, budget)));
    }
    return l;
}

}  // namespace chess
