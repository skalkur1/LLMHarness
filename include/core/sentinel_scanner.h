// include/core/sentinel_scanner.h
// Interface fixed by spec §3.4.

#pragma once
#include <string>
#include <string_view>

class SentinelScanner {
public:
    // Throws std::invalid_argument if sentinel is empty.
    explicit SentinelScanner(std::string sentinel);

    struct Out { std::string safe_text; bool sentinel_found; };

    // Feed the next chunk. Returns text guaranteed NOT to be part of
    // the sentinel (safe to print immediately) and whether the
    // sentinel has now been fully seen. Text after the sentinel is
    // discarded; once found, later feeds return {"", true}.
    Out feed(std::string_view chunk);

    // Call once, after the stream ends, to release any text still
    // being held back.
    Out flush();

    // Characters currently held back. Always <= sentinel.size() - 1.
    // Exposed so tests can check the bounded-memory guarantee.
    std::size_t pending_size() const noexcept;

private:
    std::string sentinel_;
    std::string pending_;   // holds back at most sentinel_.size() - 1
                             // trailing characters that could still
                             // become the start of the sentinel
    bool found_ = false;
};
