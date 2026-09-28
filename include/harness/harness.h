// include/harness/harness.h
// PROVIDED — do not modify.

#pragma once
#include "model/model_client.h"
#include <memory>
#include <string>

struct HarnessConfig {
    int max_turns = 20;

    // Optional: seeds the conversation with a pinned System message before
    // the first turn (see main.cpp — this is filled in from the model's
    // system_message(), when the script/transcript had one). Empty means
    // no system message.
    std::string system_message;
};

class InputSource {
public:
    virtual ~InputSource() = default;
    virtual std::string read_line() = 0;
    virtual bool is_eof() const = 0;
};

class OutputSink {
public:
    virtual ~OutputSink() = default;
    virtual void write(std::string_view text) = 0;
};

class Harness {
public:
    Harness(std::unique_ptr<ModelClient> model, HarnessConfig cfg);

    StopReason run(InputSource& in, OutputSink& out);

    const Conversation& conversation() const { return conv_; }

private:
    std::unique_ptr<ModelClient> model_;
    Conversation conv_;
    HarnessConfig cfg_;
};
