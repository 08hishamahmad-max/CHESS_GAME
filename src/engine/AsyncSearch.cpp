// AsyncSearch.cpp
#include "AsyncSearch.h"

namespace chess {

AsyncSearch::~AsyncSearch() { cancel(); }

void AsyncSearch::start(const Board& position, const SearchLimits& limits,
                        const std::vector<std::uint64_t>& history) {
    cancel();
    stop_ = false;
    finished_ = false;
    running_ = true;
    // The lambda receives copies of everything it needs, so the GUI may change
    // its own board while the worker thinks.
    worker_ = std::thread([this, position, limits, history]() {
        result_ = search_.think(position, limits, history, &stop_);
        finished_ = true;  // atomic: publishes result_ to the GUI thread
    });
}

SearchResult AsyncSearch::takeResult() {
    if (worker_.joinable()) worker_.join();
    running_ = false;
    return result_;
}

void AsyncSearch::cancel() {
    stop_ = true;
    if (worker_.joinable()) worker_.join();
    running_ = false;
    finished_ = false;
}

}  // namespace chess
