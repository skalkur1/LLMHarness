// include/model/replay_client.h
// PROVIDED — do not modify.

#pragma once
#include "model/model_client.h"
#include <string>
#include <vector>

class ReplayModelClient : public ModelClient {
public:
    using ModelClient::generate;
    explicit ReplayModelClient(const std::string& transcript_path);
    void generate(const Conversation& conv, TokenSink& sink) override;

    // The leading "role: system" block, if the transcript had one.
    const std::string& system_message() const noexcept { return system_message_; }

private:
    std::vector<std::string> assistant_replies_;
    std::size_t current_turn_ = 0;
    std::string system_message_;
};
