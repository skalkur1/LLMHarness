// src/scripted_client.cpp
// PROVIDED — do not modify.

#include "model/scripted_client.h"
#include <fstream>
#include <stdexcept>

namespace {
// Trims leading whitespace (useful for parsing "chunk: 5").
std::string trim_left(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t");
    return (start == std::string::npos) ? "" : s.substr(start);
}

enum class BlockKind { None, System, User, Assistant };
}  // namespace

ScriptedModelClient::ScriptedModelClient(const std::string& script_path) {
    std::ifstream file(script_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open script file: " + script_path);
    }

    BlockKind kind = BlockKind::None;
    int current_chunk = -1;
    std::string current_content;
    bool first_block_seen = false;

    auto finish_block = [&]() {
        if (kind == BlockKind::Assistant) {
            blocks_.push_back({current_content, current_chunk});
        } else if (kind == BlockKind::System && !first_block_seen) {
            // Appendix A: a script that BEGINS with "role: system" has that
            // block become the initial system message. Only the very first
            // block in the file counts.
            system_message_ = current_content;
        }
        if (kind != BlockKind::None) first_block_seen = true;
        kind = BlockKind::None;
        current_chunk = -1;
        current_content.clear();
    };

    std::string line;
    while (std::getline(file, line)) {
        if (line == "---") {
            finish_block();
            continue;
        }

        if (kind == BlockKind::None) {
            // Looking for directives or a role header.
            if (line.rfind("chunk:", 0) == 0) {
                try {
                    current_chunk = std::stoi(trim_left(line.substr(6)));
                } catch (...) {
                    current_chunk = -1;
                }
            } else if (line.rfind("match:", 0) == 0) {
                // Baseline implementation ignores regex branching
                // (see spec's optional stretch goal).
                continue;
            } else if (line == "role: assistant") {
                kind = BlockKind::Assistant;
            } else if (line == "role: system") {
                kind = BlockKind::System;
            } else if (line == "role: user") {
                kind = BlockKind::User;
            }
        } else {
            // Actively inside a block, accumulate its text.
            if (!current_content.empty()) current_content += "\n";
            current_content += line;
        }
    }

    // Handle EOF reached without a trailing "---".
    finish_block();
}

void ScriptedModelClient::generate(const Conversation& /*conv*/, TokenSink& sink) {
    if (current_block_idx_ >= blocks_.size()) {
        // Appendix A: exhausting the script must surface as a ClientError,
        // not as a fabricated reply. Harness::run() converts this into
        // StopReason::Kind::ClientError.
        throw std::runtime_error(
            "ScriptedModelClient: script exhausted, no more scripted replies");
    }

    const ScriptBlock& block = blocks_[current_block_idx_++];

    if (block.chunk_size > 0) {
        for (std::size_t i = 0; i < block.content.size(); i += block.chunk_size) {
            sink.on_chunk(block.content.substr(i, block.chunk_size));
        }
    } else {
        // No chunk directive: stream it all at once.
        sink.on_chunk(block.content);
    }

    sink.on_complete();
}
