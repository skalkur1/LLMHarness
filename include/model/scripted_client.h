// include/model/scripted_client.h
// PROVIDED — do not modify.

#pragma once
#include "model/model_client.h"
#include <string>
#include <vector>

class ScriptedModelClient : public ModelClient {
public:
    // Fix name hiding so the non-virtual generate(conv) is visible.
    using ModelClient::generate;

    explicit ScriptedModelClient(const std::string& script_path);

    void generate(const Conversation& conv, TokenSink& sink) override;

    // The leading "role: system" block, if the script had one. Empty
    // string if the script had none. Not part of the abstract ModelClient
    // interface — only ScriptedModelClient/ReplayModelClient expose this,
    // because it's only meaningful for the two concrete, file-backed clients.
    const std::string& system_message() const noexcept { return system_message_; }

private:
    struct ScriptBlock {
        std::string content;
        int chunk_size = -1; // -1 means emit all at once
    };

    std::vector<ScriptBlock> blocks_;
    std::size_t current_block_idx_ = 0;
    std::string system_message_;
};
