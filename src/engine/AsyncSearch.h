// AsyncSearch.h - runs the Search on a background (worker) thread so that the
// window keeps responding while the computer thinks.
//
// Usage from the GUI thread:
//     ai.start(board, limits, history);   // returns immediately
//     ... every frame: if (ai.isFinished()) { SearchResult r = ai.takeResult(); ... }
//     ai.cancel();                         // e.g. when the player presses "New game"
#pragma once

#include "Search.h"

#include <atomic>
#include <thread>
#include <vector>

namespace chess {

class AsyncSearch {
public:
    AsyncSearch() = default;
    ~AsyncSearch();
    AsyncSearch(const AsyncSearch&) = delete;
    AsyncSearch& operator=(const AsyncSearch&) = delete;

    // Starts thinking about `position`. Any running search is cancelled first.
    void start(const Board& position, const SearchLimits& limits, const std::vector<std::uint64_t>& history);

    bool isRunning() const { return running_; }                          // started and not yet taken
    bool isFinished() const { return running_ && finished_.load(); }     // result is ready

    // Returns the result of a finished search (joins the worker thread).
    SearchResult takeResult();

    // Stops the search as soon as possible and throws the result away.
    void cancel();

private:
    Search search_;   // owned here so the transposition table is kept between moves
    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> finished_{false};
    SearchResult result_;
    bool running_ = false;
};

}  // namespace chess
