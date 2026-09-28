// src/replay_client.cpp
// PROVIDED — do not modify.

#include "model/replay_client.h"
#include <fstream>
#include <stdexcept>

namespace {
enum class BlockKind { None, System, User, Assistant };
}  // namespace

ReplayModelClient::ReplayModelClient(const std::string& transcript_path) {
    std::ifstream file(transcript_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open: " + transcript_path);
    }

    BlockKind kind = BlockKind::None;
    std::string current_content;
    bool first_block_seen = false;

    auto finish_block = [&]() {
        if (kind == BlockKind::Assistant && !current_content.empty()) {
            assistant_replies_.push_back(current_content);
        } else if (kind == BlockKind::System && !first_block_seen) {
            // Appendix A: a transcript that BEGINS with "role: system" has
            // that block become the initial system message.
            system_message_ = current_content;
        }
        if (kind != BlockKind::None) first_block_seen = true;
        kind = BlockKind::None;
        current_content.clear();
    };

    std::string line;
    while (std::getline(file, line)) {
        if (line == "---") {
            finish_block();
            continue;
        }

        if (kind == BlockKind::None) {
            if (line == "role: assistant") {
                kind = BlockKind::Assistant;
            } else if (line == "role: system") {
                kind = BlockKind::System;
            } else if (line == "role: user") {
                kind = BlockKind::User;
            }
        } else {
            if (!current_content.empty()) current_content += "\n";
            current_content += line;
        }
    }

    // Handle EOF reached without a trailing "---".
    finish_block();
}

void ReplayModelClient::generate(const Conversation& /*conv*/, TokenSink& sink) {
    if (current_turn_ >= assistant_replies_.size()) {
        // Consistent with ScriptedModelClient: running out of recorded
        // turns is a client error, not a fabricated reply — this way a
        // transcript round-trip test that runs too many turns fails loudly
        // instead of silently injecting a fake sentinel.
        throw std::runtime_error(
            "ReplayModelClient: transcript exhausted, no more recorded turns");
    }

    const std::string& reply = assistant_replies_[current_turn_++];

    // Replay streams the whole recorded reply at once.
    sink.on_chunk(reply);
    sink.on_complete();
}
