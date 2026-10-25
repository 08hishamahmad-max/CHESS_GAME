// ChessClock.cpp
#include "ChessClock.h"

#include <cstdio>

namespace chess {

std::string TimeControl::toString() const {
    if (!enabled()) return "No clock";
    std::string base = (baseSeconds % 60 == 0) ? std::to_string(baseSeconds / 60)
                                               : std::to_string(baseSeconds / 60) + ":" +
                                                     (baseSeconds % 60 < 10 ? "0" : "") + std::to_string(baseSeconds % 60);
    return base + "+" + std::to_string(incrementSeconds);
}

void ChessClock::reset(const TimeControl& tc) {
    tc_ = tc;
    remaining_[0] = remaining_[1] = static_cast<long long>(tc.baseSeconds) * 1000;
}

void ChessClock::tick(Color side, long long elapsedMs) {
    if (!enabled() || elapsedMs <= 0) return;
    long long& r = remaining_[static_cast<int>(side)];
    r -= elapsedMs;
    if (r < 0) r = 0;
}

void ChessClock::moveMade(Color side) {
    if (!enabled()) return;
    remaining_[static_cast<int>(side)] += static_cast<long long>(tc_.incrementSeconds) * 1000;
}

std::string ChessClock::format(long long ms) {
    if (ms < 0) ms = 0;
    char buf[32];
    if (ms < 10000) {
        std::snprintf(buf, sizeof buf, "0:%02lld.%lld", ms / 1000, (ms % 1000) / 100);
    } else {
        long long totalSeconds = (ms + 999) / 1000;  // round up like real clocks
        long long h = totalSeconds / 3600, m = (totalSeconds / 60) % 60, s = totalSeconds % 60;
        if (h > 0)
            std::snprintf(buf, sizeof buf, "%lld:%02lld:%02lld", h, m, s);
        else
            std::snprintf(buf, sizeof buf, "%lld:%02lld", m, s);
    }
    return buf;
}

}  // namespace chess
