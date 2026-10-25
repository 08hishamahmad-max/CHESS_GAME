// ChessClock.h - a two-sided chess clock with Fischer increment.
//
// The clock does not read the system time itself: the GUI calls
// tick(elapsedMs) every frame. That keeps this class simple and testable.
#pragma once

#include "Types.h"

#include <array>
#include <string>

namespace chess {

struct TimeControl {
    int baseSeconds = 0;       // 0 = no clock
    int incrementSeconds = 0;  // added after each move

    bool enabled() const { return baseSeconds > 0; }
    std::string toString() const;  // "5+3" or "No clock"
};

class ChessClock {
public:
    void reset(const TimeControl& tc);

    bool enabled() const { return tc_.enabled(); }
    const TimeControl& timeControl() const { return tc_; }

    // Counts down the time of `side` (the side to move).
    void tick(Color side, long long elapsedMs);
    // Called after `side` completed a move: adds the increment.
    void moveMade(Color side);

    long long remainingMs(Color side) const { return remaining_[static_cast<int>(side)]; }
    void setRemainingMs(Color side, long long ms) { remaining_[static_cast<int>(side)] = ms; }
    bool flagged(Color side) const { return enabled() && remaining_[static_cast<int>(side)] <= 0; }

    // "4:59", or "0:09.3" with tenths when less than 10 seconds remain.
    static std::string format(long long ms);

private:
    TimeControl tc_;
    std::array<long long, 2> remaining_{{0, 0}};
};

}  // namespace chess
