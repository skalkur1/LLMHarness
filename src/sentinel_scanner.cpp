// src/sentinel_scanner.cpp

#include "core/sentinel_scanner.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {
    if (sentinel_.empty()) {
        throw std::invalid_argument("SentinelScanner: sentinel must not be empty");
    }
}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    if (found_) return {"", true};

    // Only pending_ (< sentinel_.size() chars) is carried over, so this
    // buffer is at most chunk.size() + sentinel_.size() - 1 long.
    std::string buf = pending_;
    buf.append(chunk);

    std::size_t pos = buf.find(sentinel_);
    if (pos != std::string::npos) {
        found_ = true;
        pending_.clear();
        buf.resize(pos);  // keep only text before the sentinel
        return {std::move(buf), true};
    }

    // No full match. Hold back the longest suffix of buf that is also a
    // prefix of the sentinel — it could still become the sentinel. A full
    // match was ruled out above, so that suffix has at most
    // sentinel_.size() - 1 characters. Everything before it is safe.
    std::size_t keep = std::min(buf.size(), sentinel_.size() - 1);
    while (keep > 0 &&
           buf.compare(buf.size() - keep, keep, sentinel_, 0, keep) != 0) {
        --keep;
    }

    pending_.assign(buf, buf.size() - keep, keep);
    buf.resize(buf.size() - keep);
    return {std::move(buf), false};
}

SentinelScanner::Out SentinelScanner::flush() {
    if (found_) return {"", true};

    // The stream ended without completing the sentinel, so whatever was
    // held back was ordinary text after all.
    std::string rest = std::move(pending_);
    pending_.clear();
    return {std::move(rest), false};
}

std::size_t SentinelScanner::pending_size() const noexcept { return pending_.size(); }
