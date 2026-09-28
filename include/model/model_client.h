// include/model/model_client.h
// PROVIDED — do not modify. Matches the ModelClient interface in spec §3.3.

#pragma once
#include "core/conversation.h"
#include <string_view>

struct StopReason {
    enum class Kind { Sentinel, TurnLimit, UserExit, ClientError } kind;
    std::string detail;
};

class TokenSink {
public:
    virtual ~TokenSink() = default;
    virtual void on_chunk(std::string_view chunk) = 0;
    virtual void on_complete() = 0;
};

class ModelClient {
public:
    virtual ~ModelClient() = default;

    virtual void generate(const Conversation& conv, TokenSink& sink) = 0;

    // NVI implementation provided in base
    Message generate(const Conversation& conv);
};
