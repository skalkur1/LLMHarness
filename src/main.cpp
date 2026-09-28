// src/main.cpp
// PROVIDED — do not modify. CLI parsing is handled for you; your work is
// Conversation and SentinelScanner (see spec §2 "Note on command-line
// arguments" and §3).

#include "harness/harness.h"
#include "model/scripted_client.h"
#include <iostream>
#include <fstream>
#include <string>

namespace {

class StdioInput : public InputSource {
public:
    std::string read_line() override {
        std::string line;
        std::getline(std::cin, line);
        eof_ = std::cin.eof();
        return line;
    }
    bool is_eof() const override { return eof_; }

private:
    bool eof_ = false;
};

class StdioOutput : public OutputSink {
public:
    void write(std::string_view text) override {
        std::cout << text << std::flush;
    }
};

const char* role_name(Role role) {
    switch (role) {
        case Role::System: return "system";
        case Role::User: return "user";
        case Role::Assistant: return "assistant";
    }
    return "assistant";
}

// Required by spec: saves the conversation in Appendix A transcript format.
void save_transcript(const Conversation& conv, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return;

    bool first = true;
    for (const Message* m = conv.begin(); m != conv.end(); ++m) {
        if (!first) file << "---\n";
        first = false;
        file << "role: " << role_name(m->role()) << "\n";
        file << m->content() << "\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string script_path = "default.script";
    std::string save_path;
    HarnessConfig config;

    // Basic CLI argument parsing.
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--script" && i + 1 < argc) script_path = argv[++i];
        else if (arg == "--save" && i + 1 < argc) save_path = argv[++i];
        else if (arg == "--max-turns" && i + 1 < argc) config.max_turns = std::stoi(argv[++i]);
    }

    StdioInput in;
    StdioOutput out;

    try {
        auto scripted_model = std::make_unique<ScriptedModelClient>(script_path);
        // Pull the system message (if any) out of the concrete client before
        // type-erasing it into ModelClient — Harness only needs the string,
        // not the concrete type.
        config.system_message = scripted_model->system_message();

        Harness harness(std::move(scripted_model), config);

        StopReason reason = harness.run(in, out);
        std::cout << "[conversation ended: " << reason.detail << "]\n";

        if (!save_path.empty()) {
            save_transcript(harness.conversation(), save_path);
            std::cout << "[Transcript saved to " << save_path << "]\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
