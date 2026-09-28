# ECE 309 — Project 2 starter code

This is the starter repo for Project 2 (see the spec for full details).

## What's provided vs. what's yours

Everything under `include/model/`, `include/harness/`, `src/model_client.cpp`,
`src/scripted_client.cpp`, `src/replay_client.cpp`, `src/harness.cpp`, and
`src/main.cpp` is given, working code — read it, don't modify it.

You write:

- `include/core/message.h` (+ optional `src/message.cpp`)
- `include/core/conversation.h` / `src/conversation.cpp`
- `include/core/sentinel_scanner.h` / `src/sentinel_scanner.cpp`
- `tests/p2/test_p2.cpp`
- `docs/design-log-p2.md`

## Build and run

```bash
cmake -S . -B build
cmake --build build
```

This builds two targets:

- `./build/miniharness` — the interactive CLI
- `./build/test_p2` — your test suite

Try it once your `Conversation` and `SentinelScanner` compile:

```bash
./build/miniharness --script scripts/greeting.script --save transcript.txt
```

Press Ctrl-D on an empty line to end the conversation early.
# LLMHarness
