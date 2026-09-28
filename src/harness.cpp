// src/harness.cpp
// PROVIDED — do not modify.

#include "harness/harness.h"
#include "core/sentinel_scanner.h"
#include <stdexcept>

namespace {

constexpr const char* kSentinel = "<|end_conversation|>";

// An internal TokenSink that pipes model output directly to the OutputSink
// while scanning it for the stop sentinel.
//
// Two different strings matter here, and they are NOT the same thing:
//   - what gets printed to the terminal: the sentinel must never appear
//     (Required Behavior #3).
//   - what gets stored in the Conversation (and therefore saved to the
//     transcript): must include the sentinel itself, so that replaying a
//     saved transcript re-triggers the same stop (Required Behavior #5,
//     Appendix A's example transcript ends in "...<|end_conversation|>").
// Anything the model streams AFTER the sentinel is discarded from both —
// once the sentinel is seen, the reply is over.
class StreamingSink : public TokenSink {
public:
    explicit StreamingSink(OutputSink& out) : out_(out), scanner_(kSentinel) {}

    void on_chunk(std::string_view chunk) override {
        if (stopped_) return;
        auto res = scanner_.feed(chunk);
        out_.write(res.safe_text);
        stored_text_ += res.safe_text;
        if (res.sentinel_found) {
            stored_text_ += kSentinel;
            stopped_ = true;
        }
    }

    void on_complete() override {
        if (!stopped_) {
            auto res = scanner_.flush();
            out_.write(res.safe_text);
            stored_text_ += res.safe_text;
        }
    }

    bool stopped() const { return stopped_; }
    const std::string& stored_text() const { return stored_text_; }

private:
    OutputSink& out_;
    SentinelScanner scanner_;
    bool stopped_ = false;
    std::string stored_text_;
};
}  // namespace

Harness::Harness(std::unique_ptr<ModelClient> model, HarnessConfig cfg)
    : model_(std::move(model)), cfg_(std::move(cfg)) {
    // Appendix A: a script/transcript that begins with "role: system" seeds
    // the conversation's pinned system message, first, before any turns run.
    if (!cfg_.system_message.empty()) {
        conv_.append(Message(Role::System, cfg_.system_message));
    }
}

StopReason Harness::run(InputSource& in, OutputSink& out) {
    int turns = 0;

    while (turns < cfg_.max_turns) {
        out.write("you> ");
        std::string user_text = in.read_line();

        if (in.is_eof()) return {StopReason::Kind::UserExit, "EOF detected"};
        if (user_text.empty()) continue;

        conv_.append(Message(Role::User, user_text));

        out.write("assistant> ");
        StreamingSink sink(out);

        try {
            model_->generate(conv_, sink);
        } catch (const std::exception& e) {
            out.write("\n");
            return {StopReason::Kind::ClientError, e.what()};
        }
        out.write("\n");

        conv_.append(Message(Role::Assistant, sink.stored_text()));

        if (sink.stopped()) {
            return {StopReason::Kind::Sentinel,
                    "stop sentinel after " + std::to_string(turns + 1) + " turns"};
        }
        turns++;
    }
    return {StopReason::Kind::TurnLimit, "Max turn limit reached"};
}
