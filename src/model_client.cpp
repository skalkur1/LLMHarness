// src/model_client.cpp
// PROVIDED — do not modify.

#include "model/model_client.h"

// A simple sink to collect the full string when we don't care about streaming.
namespace {
class StringSink : public TokenSink {
public:
    std::string content;
    void on_chunk(std::string_view chunk) override { content += chunk; }
    void on_complete() override {}
};
}  // namespace

Message ModelClient::generate(const Conversation& conv) {
    StringSink sink;
    generate(conv, sink);
    return Message(Role::Assistant, std::move(sink.content));
}
