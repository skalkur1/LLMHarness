// tests/p2/test_p2.cpp
//
// Assert-based test suite for Project 2. Covers every category in spec §5:
//   1-5   Conversation (bounds, system pinning, copy, move, growth)
//   6-9   SentinelScanner (clean text, every split, false alarms, bounded memory)
//   10-12 Provided Harness running on top of our classes (turn limit,
//         sentinel halt, transcript round-trip), plus EOF shutdown.
//
// Build with CMake (ASan/UBSan are on) and run ./build/test_p2.

#undef NDEBUG  // asserts must always fire, even in a Release build
#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal test registry: TEST(Name) { ... } defines and registers a test.
// ---------------------------------------------------------------------------
namespace {

std::vector<std::pair<const char*, std::function<void()>>>& registry() {
    static std::vector<std::pair<const char*, std::function<void()>>> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().emplace_back(name, std::move(fn));
    }
};

}  // namespace

#define TEST(name)                                        \
    static void test_##name();                            \
    static Registrar registrar_##name(#name, test_##name); \
    static void test_##name()

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

const std::string kSentinel = "<|end_conversation|>";

// Returns preset lines, then reports EOF.
class VectorInput : public InputSource {
public:
    explicit VectorInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    std::string read_line() override {
        if (next_ >= lines_.size()) {
            eof_ = true;
            return "";
        }
        return lines_[next_++];
    }
    bool is_eof() const override { return eof_; }

private:
    std::vector<std::string> lines_;
    std::size_t next_ = 0;
    bool eof_ = false;
};

// Captures everything the harness prints.
class StringOutput : public OutputSink {
public:
    void write(std::string_view text) override { text_ += text; }
    const std::string& text() const { return text_; }

private:
    std::string text_;
};

// A file in the system temp directory, deleted when the object goes away.
class TempFile {
public:
    TempFile(const std::string& name, const std::string& contents)
        : path_((std::filesystem::temp_directory_path() / name).string()) {
        std::ofstream out(path_);
        out << contents;
    }
    ~TempFile() { std::remove(path_.c_str()); }
    const std::string& path() const { return path_; }

private:
    std::string path_;
};

const char* role_name(Role role) {
    switch (role) {
        case Role::System: return "system";
        case Role::User: return "user";
        case Role::Assistant: return "assistant";
    }
    return "assistant";
}

// Same format as main.cpp's save_transcript (spec Appendix A).
std::string to_transcript(const Conversation& conv) {
    std::string out;
    bool first = true;
    for (const Message& m : conv) {
        if (!first) out += "---\n";
        first = false;
        out += "role: ";
        out += role_name(m.role());
        out += "\n" + m.content() + "\n";
    }
    return out;
}

// Feeds every chunk, flushes if the sentinel never appeared, and returns
// all safe text. found is set if any call reported the sentinel.
std::string scan_all(const std::vector<std::string>& chunks, bool& found) {
    SentinelScanner scanner(kSentinel);
    std::string safe;
    found = false;
    for (const std::string& chunk : chunks) {
        auto out = scanner.feed(chunk);
        safe += out.safe_text;
        found = found || out.sentinel_found;
    }
    if (!found) safe += scanner.flush().safe_text;
    return safe;
}

const std::string kScript =
    "role: system\n"
    "Be concise.\n"
    "---\n"
    "chunk: 3\n"
    "role: assistant\n"
    "Hi there!\n"
    "---\n"
    "chunk: 1\n"
    "role: assistant\n"
    "Goodbye.<|end_conversation|>ignored tail\n"
    "---\n"
    "role: assistant\n"
    "This reply should never be reached.\n";

}  // namespace

// ===========================================================================
// 1-5: Conversation
// ===========================================================================

// 1. Empty conversation: no allocation, empty range, at() is bounds-checked.
TEST(EmptyConversationBounds) {
    Conversation conv;
    assert(conv.size() == 0);
    assert(conv.capacity() == 0);
    assert(conv.begin() == nullptr);
    assert(conv.begin() == conv.end());

    int visited = 0;
    for (const Message& m : conv) {
        (void)m;
        ++visited;
    }
    assert(visited == 0);

    bool threw = false;
    try {
        conv.at(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw && "at(0) on an empty conversation must throw");
}

// 1b. at() on a non-empty conversation throws exactly at size().
TEST(AtThrowsPastEnd) {
    Conversation conv;
    conv.append(Message(Role::User, "a"));
    conv.append(Message(Role::Assistant, "b"));
    assert(conv.at(1).content() == "b");

    bool threw = false;
    try {
        conv.at(2);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw);
}

// 2. The system message stays at index 0 and a second one is rejected.
TEST(SystemMessagePinnedFirst) {
    Conversation conv;
    conv.append(Message(Role::System, "Be concise."));
    for (int i = 0; i < 10; ++i) {
        conv.append(Message(i % 2 == 0 ? Role::User : Role::Assistant,
                            "turn " + std::to_string(i)));
    }
    assert(conv.at(0).role() == Role::System);
    assert(conv.at(0).content() == "Be concise.");
    assert(conv.begin()->role() == Role::System);

    bool threw = false;
    try {
        conv.append(Message(Role::System, "late system message"));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw && "a System message after index 0 must be rejected");
    assert(conv.size() == 11 && "a rejected append must not change size");

    // A system message is also rejected when it isn't the very first append.
    Conversation user_first;
    user_first.append(Message(Role::User, "hi"));
    threw = false;
    try {
        user_first.append(Message(Role::System, "too late"));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// 3. Copy constructor and copy assignment make independent deep copies.
TEST(CopyIsDeep) {
    Conversation original;
    original.append(Message(Role::System, "sys"));
    original.append(Message(Role::User, "hello"));
    original.append(Message(Role::Assistant, "hi"));

    Conversation copy(original);
    assert(copy.size() == original.size());
    assert(copy.begin() != original.begin() && "copy must own its own buffer");
    for (std::size_t i = 0; i < original.size(); ++i) {
        assert(copy.at(i).role() == original.at(i).role());
        assert(copy.at(i).content() == original.at(i).content());
        assert(&copy.at(i) != &original.at(i));
    }

    // Growing the copy must not affect the original.
    copy.append(Message(Role::User, "only in copy"));
    assert(copy.size() == 4);
    assert(original.size() == 3);

    // Copy assignment over a non-empty target.
    Conversation assigned;
    assigned.append(Message(Role::User, "will be replaced"));
    assigned = original;
    assert(assigned.size() == 3);
    assert(assigned.begin() != original.begin());
    assert(assigned.at(1).content() == "hello");

    // Copying an empty conversation stays unallocated.
    Conversation empty;
    Conversation empty_copy(empty);
    assert(empty_copy.size() == 0 && empty_copy.begin() == nullptr);
}

// 3b. Self-assignment is a no-op and does not corrupt or free the buffer.
TEST(CopySelfAssignment) {
    Conversation conv;
    conv.append(Message(Role::User, "keep me"));
    const Message* before = conv.begin();

    Conversation& alias = conv;
    conv = alias;
    assert(conv.size() == 1);
    assert(conv.begin() == before);
    assert(conv.at(0).content() == "keep me");
}

// 4. Move constructor and move assignment steal the buffer and empty the source.
TEST(MoveStealsBuffer) {
    Conversation source;
    source.append(Message(Role::User, "a"));
    source.append(Message(Role::Assistant, "b"));
    source.append(Message(Role::User, "c"));
    const Message* buffer = source.begin();
    const std::size_t capacity = source.capacity();

    Conversation moved(std::move(source));
    assert(moved.begin() == buffer && "move must steal, not copy");
    assert(moved.size() == 3);
    assert(moved.capacity() == capacity);
    assert(moved.at(2).content() == "c");
    assert(source.begin() == nullptr);
    assert(source.size() == 0);
    assert(source.capacity() == 0);

    // Move assignment over a non-empty target (its old buffer must be freed;
    // ASan reports a leak if it isn't).
    Conversation target;
    target.append(Message(Role::User, "old contents"));
    target = std::move(moved);
    assert(target.begin() == buffer);
    assert(target.size() == 3);
    assert(moved.begin() == nullptr && moved.size() == 0 && moved.capacity() == 0);

    // A moved-from object is still usable.
    moved.append(Message(Role::User, "reused"));
    assert(moved.size() == 1 && moved.at(0).content() == "reused");
    moved = target;
    assert(moved.size() == 3 && moved.begin() != target.begin());

    // Self move-assignment leaves the object intact.
    Conversation& alias = target;
    target = std::move(alias);
    assert(target.size() == 3 && target.begin() == buffer);
}

// 5. Capacity doubles (1, 2, 4, 8, ...) and contents survive every reallocation.
TEST(GrowthDoublesAndPreservesContents) {
    Conversation conv;
    std::size_t reallocations = 0;
    const std::size_t n = 1000;

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t old_capacity = conv.capacity();
        conv.append(Message(i % 2 == 0 ? Role::User : Role::Assistant,
                            "message " + std::to_string(i)));

        if (old_capacity == i) {  // was full, so append had to grow
            ++reallocations;
            assert(conv.capacity() == (old_capacity == 0 ? 1 : old_capacity * 2));
        } else {
            assert(conv.capacity() == old_capacity && "no growth unless full");
        }
        assert(conv.size() == i + 1);
        assert(conv.capacity() >= conv.size());

        // Every earlier message is still correct after a reallocation.
        if (old_capacity == i) {
            for (std::size_t j = 0; j <= i; ++j) {
                assert(conv.at(j).content() == "message " + std::to_string(j));
            }
        }
    }
    // 1000 appends with doubling: capacities 1, 2, 4, ..., 1024 = 11 allocations.
    assert(reallocations == 11);
    assert(conv.capacity() == 1024);

    // Range-for visits them in order.
    std::size_t index = 0;
    for (const Message& m : conv) {
        assert(m.content() == "message " + std::to_string(index));
        ++index;
    }
    assert(index == n);
}

// ===========================================================================
// 6-9: SentinelScanner
// ===========================================================================

// 6. Text without the sentinel passes through unchanged, in any chunking.
TEST(ScannerCleanText) {
    const std::string text = "Hello! This reply has no stop marker at all.\nSecond line.";

    bool found = false;
    assert(scan_all({text}, found) == text);
    assert(!found);

    // One byte at a time.
    std::vector<std::string> bytes;
    for (char c : text) bytes.emplace_back(1, c);
    assert(scan_all(bytes, found) == text);
    assert(!found);

    // Plain text with no '<' is emitted immediately, not held back.
    SentinelScanner scanner(kSentinel);
    auto out = scanner.feed("hello");
    assert(out.safe_text == "hello");
    assert(scanner.pending_size() == 0);

    // Empty chunks are harmless.
    assert(scan_all({"", "abc", "", ""}, found) == "abc");
    assert(!found);
}

// 7. The spec's sample test: the sentinel is caught at every two-way split.
TEST(ScannerCatchesSentinelAtEveryBoundary) {
    const std::string text = "Goodbye." + kSentinel;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(kSentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert((out1.sentinel_found || out2.sentinel_found) &&
               "sentinel must be caught regardless of split point");
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
}

// 7b. Every three-way split, with a partial-match decoy before the sentinel
//     and junk after it (which must be discarded).
TEST(ScannerCatchesSentinelAtEveryThreeWaySplit) {
    const std::string text = "ok <|end_" + kSentinel + "trailing junk";
    const std::string expected = "ok <|end_";
    for (std::size_t i = 0; i <= text.size(); ++i) {
        for (std::size_t j = i; j <= text.size(); ++j) {
            bool found = false;
            std::string safe = scan_all(
                {text.substr(0, i), text.substr(i, j - i), text.substr(j)}, found);
            assert(found);
            assert(safe == expected);
        }
    }
}

// 7c. After the sentinel, later feeds and flush emit nothing.
TEST(ScannerStopsAfterSentinel) {
    SentinelScanner scanner(kSentinel);
    auto out = scanner.feed("bye" + kSentinel + "after");
    assert(out.sentinel_found && out.safe_text == "bye");

    auto later = scanner.feed("more text");
    assert(later.safe_text.empty() && later.sentinel_found);
    auto flushed = scanner.flush();
    assert(flushed.safe_text.empty());
}

// 8. Near-misses never trigger, and none of their text is lost.
TEST(ScannerFalseAlarms) {
    const std::vector<std::string> decoys = {
        "<|end_world|>",
        "<|end_conversation|",        // missing final '>'
        "<|end_conversatio",          // stream ends mid-prefix
        "<|end_conversation >",       // wrong character near the end
        "<<||end_conversation||>>",
        "<|END_CONVERSATION|>",       // case differs
        "end_conversation|>",         // missing opening
        "<",
    };
    for (const std::string& decoy : decoys) {
        const std::string text = "before " + decoy + " after";

        bool found = false;
        assert(scan_all({text}, found) == text);
        assert(!found);

        std::vector<std::string> bytes;
        for (char c : text) bytes.emplace_back(1, c);
        assert(scan_all(bytes, found) == text);
        assert(!found);
    }

    // A decoy prefix at the very end is released only by flush().
    SentinelScanner scanner(kSentinel);
    auto out = scanner.feed("wait <|end_con");
    assert(!out.sentinel_found);
    assert(out.safe_text == "wait ");
    assert(scanner.pending_size() == std::string("<|end_con").size());
    auto flushed = scanner.flush();
    assert(!flushed.sentinel_found);
    assert(flushed.safe_text == "<|end_con");
    assert(scanner.pending_size() == 0);
}

// 9. pending_ never exceeds sentinel.size() - 1 over a 4 MB adversarial
//    stream fed one byte at a time, and no text is lost.
TEST(ScannerBoundedMemory) {
    const std::size_t bound = kSentinel.size() - 1;
    // The longest possible near-miss, repeated: every byte keeps the scanner
    // as close to a match as it can get without completing one.
    const std::string near_miss = kSentinel.substr(0, bound);
    const std::size_t target = 4u << 20;  // 4 MB

    SentinelScanner scanner(kSentinel);
    std::size_t fed = 0;
    std::size_t emitted = 0;
    std::size_t max_pending = 0;

    while (fed < target) {
        for (char c : near_miss) {
            auto out = scanner.feed(std::string_view(&c, 1));
            assert(!out.sentinel_found);
            emitted += out.safe_text.size();
            ++fed;
            assert(scanner.pending_size() <= bound);
            if (scanner.pending_size() > max_pending) max_pending = scanner.pending_size();
        }
    }
    emitted += scanner.flush().safe_text.size();

    assert(emitted == fed && "every byte must eventually be emitted");
    assert(max_pending == bound && "this stream should hit the bound exactly");

    // The sentinel is still caught right after all that.
    for (char c : kSentinel) {
        auto out = scanner.feed(std::string_view(&c, 1));
        if (c == kSentinel.back()) assert(out.sentinel_found);
    }
}

// 9b. An empty sentinel is rejected (size() - 1 would underflow).
TEST(ScannerRejectsEmptySentinel) {
    bool threw = false;
    try {
        SentinelScanner scanner("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// ===========================================================================
// 10-12: The provided Harness running on our Conversation and SentinelScanner
// ===========================================================================

// 10. With max_turns = 1, the loop stops with TurnLimit before the sentinel.
TEST(HarnessTurnLimit) {
    TempFile script("p2_turn_limit.script", kScript);
    auto model = std::make_unique<ScriptedModelClient>(script.path());
    HarnessConfig cfg;
    cfg.max_turns = 1;
    cfg.system_message = model->system_message();

    Harness harness(std::move(model), cfg);
    VectorInput in({"hello", "bye", "extra"});
    StringOutput out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::TurnLimit);
    // system + one user turn + one assistant reply
    assert(harness.conversation().size() == 3);
    assert(harness.conversation().at(0).role() == Role::System);
    assert(harness.conversation().at(2).content() == "Hi there!");
}

// 11. The loop halts on exactly the turn whose reply contains the sentinel,
//     the sentinel never reaches the output, and text after it is dropped.
TEST(HarnessSentinelHalt) {
    TempFile script("p2_sentinel.script", kScript);
    auto model = std::make_unique<ScriptedModelClient>(script.path());
    HarnessConfig cfg;
    cfg.system_message = model->system_message();

    Harness harness(std::move(model), cfg);
    VectorInput in({"hello", "bye", "should not be read"});
    StringOutput out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::Sentinel);
    assert(reason.detail == "stop sentinel after 2 turns");
    assert(out.text().find("Goodbye.") != std::string::npos);
    assert(out.text().find("<|") == std::string::npos && "sentinel must not be printed");
    assert(out.text().find("ignored tail") == std::string::npos);
    assert(out.text().find("never be reached") == std::string::npos);

    // The stored reply keeps the sentinel so a replay stops at the same point.
    const Conversation& conv = harness.conversation();
    assert(conv.size() == 5);  // system, user, assistant, user, assistant
    assert(conv.at(4).role() == Role::Assistant);
    assert(conv.at(4).content() == "Goodbye." + kSentinel);
}

// 11b. EOF on input ends the conversation with UserExit.
TEST(HarnessEofIsUserExit) {
    TempFile script("p2_eof.script", kScript);
    auto model = std::make_unique<ScriptedModelClient>(script.path());
    HarnessConfig cfg;
    cfg.system_message = model->system_message();

    Harness harness(std::move(model), cfg);
    VectorInput in({"hello"});  // one turn, then EOF
    StringOutput out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::UserExit);
    assert(harness.conversation().size() == 3);
}

// 12. Save a conversation, replay it through ReplayModelClient, and get back
//     an identical conversation and identical printed output.
TEST(TranscriptRoundTrip) {
    TempFile script("p2_round_trip.script", kScript);
    const std::vector<std::string> user_lines = {"hello", "bye"};

    auto scripted = std::make_unique<ScriptedModelClient>(script.path());
    HarnessConfig cfg;
    cfg.system_message = scripted->system_message();
    Harness original(std::move(scripted), cfg);
    VectorInput in1(user_lines);
    StringOutput out1;
    StopReason r1 = original.run(in1, out1);
    assert(r1.kind == StopReason::Kind::Sentinel);

    TempFile transcript("p2_round_trip.transcript", to_transcript(original.conversation()));

    auto replay = std::make_unique<ReplayModelClient>(transcript.path());
    HarnessConfig replay_cfg;
    replay_cfg.system_message = replay->system_message();
    assert(replay_cfg.system_message == "Be concise.");
    Harness replayed(std::move(replay), replay_cfg);
    VectorInput in2(user_lines);
    StringOutput out2;
    StopReason r2 = replayed.run(in2, out2);

    assert(r2.kind == StopReason::Kind::Sentinel);
    assert(r2.detail == r1.detail);
    assert(out2.text() == out1.text());

    const Conversation& a = original.conversation();
    const Conversation& b = replayed.conversation();
    assert(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        assert(a.at(i).role() == b.at(i).role());
        assert(a.at(i).content() == b.at(i).content());
    }
    assert(to_transcript(b) == to_transcript(a));
}

// ---------------------------------------------------------------------------

int main() {
    for (const auto& [name, fn] : registry()) {
        std::cout << "[ RUN  ] " << name << std::endl;
        fn();
        std::cout << "[  OK  ] " << name << std::endl;
    }
    std::cout << "All " << registry().size() << " tests passed.\n";
    return 0;
}
